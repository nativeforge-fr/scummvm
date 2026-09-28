// ============================================================================
//  vfimport — Native Forge portable data importer for Versailles 1685.
//
//  Reimplements, in dependency-free C++17, the core of the Windows PowerShell
//  installer's extraction pipeline so the SAME logic runs on Windows, Linux
//  (ArmadaOS / handhelds) and Android:
//
//    * ISO9660 reader with MODE2/2352 raw-sector de-raw (no 7z, no .NET);
//    * edition fingerprint (MD5 of 14 signature files, first 8 hex each);
//    * disc type detection (edition CD1 / data CD2 / multilang DVD);
//    * base extraction (DATAS_V + INSTALL, with INSTALL\DATAS_V -> INSTALL\DATA)
//      into a "game_data" folder the standalone engine boots directly.
//
//  No game data is shipped: it reads the user's own CD/ISO. This is a library
//  core; platform GUIs (Linux installer, Android app) call into it.
//
//  Commands:
//    vfimport fingerprint <iso> [dvd-prefix]
//    vfimport type        <iso>
//    vfimport list        <iso> [prefix]
//    vfimport extract     <out_game_data_dir> <iso> [<iso2> ...]
// ============================================================================
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  include <unistd.h>
#  define MKDIR(p) mkdir(p, 0755)
#endif

// ---------------------------------------------------------------------------
//  MD5 (public-domain style, RFC 1321). Outputs UPPERCASE hex to match the
//  PowerShell Get-FileHash fingerprint byte-for-byte.
// ---------------------------------------------------------------------------
namespace md5 {
struct Ctx { uint32_t a,b,c,d; uint64_t len; uint8_t buf[64]; size_t idx; };
static inline uint32_t rol(uint32_t x,int c){ return (x<<c)|(x>>(32-c)); }
static void init(Ctx&x){ x.a=0x67452301;x.b=0xefcdab89;x.c=0x98badcfe;x.d=0x10325476;x.len=0;x.idx=0; }
static void block(Ctx&x,const uint8_t*p){
  static const uint32_t K[64]={
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
  static const int S[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
                          5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                          4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
                          6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
  uint32_t M[16];
  for(int i=0;i<16;i++) M[i]=(uint32_t)p[i*4]|((uint32_t)p[i*4+1]<<8)|((uint32_t)p[i*4+2]<<16)|((uint32_t)p[i*4+3]<<24);
  uint32_t A=x.a,B=x.b,C=x.c,D=x.d;
  for(int i=0;i<64;i++){
    uint32_t F; int g;
    if(i<16){F=(B&C)|(~B&D);g=i;}
    else if(i<32){F=(D&B)|(~D&C);g=(5*i+1)&15;}
    else if(i<48){F=B^C^D;g=(3*i+5)&15;}
    else {F=C^(B|~D);g=(7*i)&15;}
    F=F+A+K[i]+M[g]; A=D; D=C; C=B; B=B+rol(F,S[i]);
  }
  x.a+=A;x.b+=B;x.c+=C;x.d+=D;
}
static void update(Ctx&x,const uint8_t*p,size_t n){
  x.len+=n;
  while(n){ size_t t=64-x.idx; if(t>n)t=n; memcpy(x.buf+x.idx,p,t); x.idx+=t; p+=t; n-=t;
    if(x.idx==64){ block(x,x.buf); x.idx=0; } }
}
static std::string hex8(Ctx&x){
  uint64_t bits=x.len*8; uint8_t pad=0x80; update(x,&pad,1);
  uint8_t z=0; while(x.idx!=56) update(x,&z,1);
  uint8_t lb[8]; for(int i=0;i<8;i++) lb[i]=(uint8_t)(bits>>(8*i)); update(x,lb,8);
  uint8_t d[16]; uint32_t v[4]={x.a,x.b,x.c,x.d};
  for(int i=0;i<4;i++) for(int j=0;j<4;j++) d[i*4+j]=(uint8_t)(v[i]>>(8*j));
  static const char*H="0123456789ABCDEF"; char o[9];
  for(int i=0;i<4;i++){ o[i*2]=H[d[i]>>4]; o[i*2+1]=H[d[i]&15]; } o[8]=0;
  return std::string(o); // first 8 hex chars (first 4 bytes), matching Substring(0,8)
}
} // namespace md5

// ---------------------------------------------------------------------------
//  ISO9660 reader (+ MODE2/2352 raw de-raw). Paths are UPPERCASE, backslash
//  separated, matching the ISO directory records and the PowerShell pipeline.
// ---------------------------------------------------------------------------
struct IsoEntry { std::string path; bool isDir; uint32_t lba; uint32_t size; };

class Iso {
public:
  bool open(const std::string& file){
    f_.open(file, std::ios::binary);
    if(!f_) return false;
    uint8_t head[16]={0};
    f_.read((char*)head,16);
    // Raw MODE2/2352 sync: 00 FF..FF 00  (Test-RawMode2: b0==0,b1==0xFF,b11==0)
    bool raw = (head[0]==0 && head[1]==0xFF && head[11]==0);
    if(raw){ phys_=2352; userOff_=24; }
    else   { phys_=2048; userOff_=0; }
    // Verify the Primary Volume Descriptor at logical sector 16 ("CD001").
    std::vector<uint8_t> pvd;
    if(!readLogical(16,1,pvd) || !hasCD001(pvd)){
      // Fallback: raw MODE1/2352 (user data at offset 16) if MODE2 guess was wrong.
      if(raw){ userOff_=16; if(!readLogical(16,1,pvd) || !hasCD001(pvd)) return false; }
      else return false;
    }
    // Root directory record at offset 156 of the PVD.
    const uint8_t* rd = &pvd[156];
    rootLba_  = le32(rd+2);
    rootSize_ = le32(rd+10);
    return true;
  }

  // Walk the whole tree once; cache the flat entry list.
  const std::vector<IsoEntry>& entries(){
    if(!walked_){ walk(rootLba_, rootSize_, ""); walked_=true; }
    return entries_;
  }

  // Extract one file (by uppercase backslash path) to memory. Returns false if absent.
  bool readFile(const std::string& path, std::vector<uint8_t>& out){
    for(const auto& e : entries())
      if(!e.isDir && e.path==path){ return readData(e.lba, e.size, out); }
    return false;
  }

private:
  std::ifstream f_;
  int phys_=2048, userOff_=0;
  uint32_t rootLba_=0, rootSize_=0;
  bool walked_=false;
  std::vector<IsoEntry> entries_;

  static uint32_t le32(const uint8_t* p){ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
  static bool hasCD001(const std::vector<uint8_t>& s){ return s.size()>=6 && s[1]=='C'&&s[2]=='D'&&s[3]=='0'&&s[4]=='0'&&s[5]=='1'; }

  bool readLogical(uint32_t lba, uint32_t count, std::vector<uint8_t>& out){
    out.resize((size_t)count*2048);
    for(uint32_t i=0;i<count;i++){
      std::streamoff off = (std::streamoff)(lba+i)*phys_ + userOff_;
      f_.clear(); f_.seekg(off, std::ios::beg);
      f_.read((char*)&out[(size_t)i*2048], 2048);
      if(f_.gcount()!=2048) { /* tolerate short tail */ }
    }
    return true;
  }
  // Read `size` bytes of file data starting at logical block `lba`.
  bool readData(uint32_t lba, uint32_t size, std::vector<uint8_t>& out){
    uint32_t blocks=(size+2047)/2048;
    std::vector<uint8_t> raw;
    if(!readLogical(lba, blocks, raw)) return false;
    out.assign(raw.begin(), raw.begin()+size);
    return true;
  }

  void walk(uint32_t lba, uint32_t size, const std::string& prefix){
    std::vector<uint8_t> dir;
    uint32_t blocks=(size+2047)/2048;
    readLogical(lba, blocks, dir);
    size_t pos=0;
    while(pos < dir.size()){
      uint8_t recLen = dir[pos];
      if(recLen==0){
        // No more records in this logical block; advance to next block boundary.
        size_t next = ((pos/2048)+1)*2048;
        if(next<=pos) break;
        pos=next; continue;
      }
      if(pos+recLen>dir.size()) break;
      const uint8_t* r=&dir[pos];
      uint8_t flags = r[25];
      uint8_t idLen = r[32];
      uint32_t eLba = le32(r+2);
      uint32_t eSize= le32(r+10);
      std::string name((const char*)r+33, idLen);
      pos += recLen;
      // Skip "." (0x00) and ".." (0x01) special entries.
      if(idLen==1 && (name[0]==0 || name[0]==1)) continue;
      // Strip ISO9660 version suffix ";1".
      size_t semi=name.find(';'); if(semi!=std::string::npos) name=name.substr(0,semi);
      if(name.empty()) continue;
      bool isDir = (flags & 0x02)!=0;
      std::string full = prefix.empty()? name : prefix+"\\"+name;
      entries_.push_back({full, isDir, eLba, eSize});
      if(isDir) walk(eLba, eSize, full);
    }
  }
};

// ---------------------------------------------------------------------------
//  Versailles-specific logic (mirrors lib_pipeline.ps1 / lib_build.ps1).
// ---------------------------------------------------------------------------
static const char* SIG_FILES[14] = {
  "DATAS_V\\OBJETS\\ESQ4T.HLZ","DATAS_V\\OBJETS\\GRAV2.HLZ","DATAS_V\\OBJETS\\PAMA.HLZ",
  "DATAS_V\\OBJETS\\PAMG.HLZ","DATAS_V\\OBJETS\\PAML.HLZ","DATAS_V\\OBJETS\\PAMM2.HLZ",
  "DATAS_V\\OBJETS\\PAMP.HLZ","DATAS_V\\OBJETS\\PAMR1.HLZ","DATAS_V\\OBJETS\\PAMR2.HLZ",
  "DATAS_V\\OBJETS\\PAMR3.HLZ","DATAS_V\\OBJETS\\PAMR4.HLZ","DATAS_V\\OBJETS\\PAPT_2.HLZ",
  "DATAS_V\\OBJETS\\VAU.HLZ","DATAS_V\\SC_TRANS\\A0_VF.HNS"
};

static std::string fingerprint(Iso& iso, const std::string& prefix){
  std::string out;
  for(int i=0;i<14;i++){
    std::string rel = prefix.empty()? SIG_FILES[i] : prefix+"\\"+SIG_FILES[i];
    std::vector<uint8_t> data;
    if(iso.readFile(rel, data)){
      md5::Ctx c; md5::init(c); md5::update(c, data.data(), data.size());
      out += md5::hex8(c);
    } else out += "--------";
  }
  return out;
}

static std::string upper(std::string s){ for(char&c:s) c=(char)toupper((unsigned char)c); return s; }

// INSTALL\DATAS_V\...  ->  INSTALL\DATA\...   (the runtime expects INSTALL\DATA)
static std::string normalizeInstall(const std::string& rel){
  const std::string from="INSTALL\\DATAS_V\\";
  if(upper(rel).rfind(from,0)==0) return "INSTALL\\DATA\\"+rel.substr(from.size());
  return rel;
}

static bool startsWith(const std::string& s,const std::string& p){ return upper(s).rfind(upper(p),0)==0; }

static void makedirs(const std::string& path){
  std::string cur;
  for(size_t i=0;i<path.size();i++){
    char c=path[i];
    if(c=='/'||c=='\\'){ if(!cur.empty()&&cur.back()!=':') MKDIR(cur.c_str()); cur+='/'; }
    else cur+=c;
  }
}

static bool writeFile(const std::string& path, const std::vector<uint8_t>& data){
  std::string dir=path; size_t s=dir.find_last_of("/\\"); if(s!=std::string::npos) dir=dir.substr(0,s); else dir=".";
  makedirs(dir+"/");
  std::ofstream o(path, std::ios::binary); if(!o) return false;
  if(!data.empty()) o.write((const char*)data.data(), data.size());
  return (bool)o;
}

// Detect disc type by top-level directories (edition CD1 / data CD2 / multilang DVD).
static std::string detectType(Iso& iso){
  static const std::pair<const char*,const char*> DVD[8]={
    {"FRANCE","fr"},{"US","en"},{"DEUTSCH","de"},{"ITALIE","it"},
    {"BRESIL","br"},{"JAPON","ja"},{"KOREE","ko"},{"CHINE","zh"}};
  std::map<std::string,bool> top; bool hasInstall=false;
  for(const auto& e:iso.entries()){
    std::string seg=e.path; size_t b=seg.find('\\'); if(b!=std::string::npos) seg=seg.substr(0,b);
    top[upper(seg)]=true;
  }
  hasInstall = top.count("INSTALL")>0;
  std::vector<std::string> langs;
  for(auto& d:DVD) if(top.count(d.first)) langs.push_back(d.second);
  if(langs.size()>=2){ std::string s="multilang"; for(auto&l:langs) s+=" "+l; return s; }
  if(hasInstall) return "edition";
  return "data";
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv){
  if(argc<3){
    fprintf(stderr,
      "vfimport (Native Forge) — Versailles data importer\n"
      "  vfimport fingerprint <iso> [dvd-prefix]\n"
      "  vfimport type        <iso>\n"
      "  vfimport list        <iso> [prefix]\n"
      "  vfimport extract     <out_game_data_dir> <iso> [<iso2> ...]\n");
    return 2;
  }
  std::string cmd=argv[1];

  if(cmd=="fingerprint"){
    Iso iso; if(!iso.open(argv[2])){ fprintf(stderr,"cannot open ISO: %s\n",argv[2]); return 1; }
    std::string prefix = (argc>=4)? argv[3] : "";
    printf("%s\n", fingerprint(iso, prefix).c_str());
    return 0;
  }
  if(cmd=="type"){
    Iso iso; if(!iso.open(argv[2])){ fprintf(stderr,"cannot open ISO: %s\n",argv[2]); return 1; }
    printf("%s\n", detectType(iso).c_str());
    return 0;
  }
  if(cmd=="list"){
    Iso iso; if(!iso.open(argv[2])){ fprintf(stderr,"cannot open ISO: %s\n",argv[2]); return 1; }
    std::string prefix = (argc>=4)? upper(argv[3]) : "";
    size_t n=0;
    for(const auto& e:iso.entries()){
      if(e.isDir) continue;
      if(!prefix.empty() && !startsWith(e.path, prefix)) continue;
      printf("%10u  %s\n", e.size, e.path.c_str()); n++;
    }
    fprintf(stderr,"(%zu files)\n", n);
    return 0;
  }
  if(cmd=="extract"){
    if(argc<4){ fprintf(stderr,"extract needs <out_dir> <iso...>\n"); return 2; }
    std::string out=argv[2];
    // game_data root; the standalone engine boots whatever is under it.
    std::string gd = out;
    size_t total=0, written=0;
    for(int a=3;a<argc;a++){
      Iso iso; if(!iso.open(argv[a])){ fprintf(stderr,"skip (cannot open): %s\n",argv[a]); continue; }
      for(const auto& e:iso.entries()){
        if(e.isDir) continue;
        // Base = everything UNDER the DATAS_V\ and INSTALL\ directories (the trailing
        // separator avoids matching sibling root files like INSTALL.EXE).
        if(!startsWith(e.path,"DATAS_V\\") && !startsWith(e.path,"INSTALL\\")) continue;
        std::string rel = normalizeInstall(e.path);
        // to filesystem path
        std::string fsrel=rel; for(char&c:fsrel) if(c=='\\') c='/';
        std::string dst = gd + "/" + fsrel;
        total++;
        std::vector<uint8_t> data;
        if(!iso.readFile(e.path,data)){ fprintf(stderr,"read fail: %s\n",e.path.c_str()); continue; }
        if(writeFile(dst,data)) written++;
      }
    }
    fprintf(stderr,"extracted %zu/%zu files into %s\n", written, total, gd.c_str());
    return (written>0)?0:1;
  }
  fprintf(stderr,"unknown command: %s\n", cmd.c_str());
  return 2;
}
