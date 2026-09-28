// ============================================================================
//  Native Forge — Versailles 1685 installer for Linux (Qt6 Widgets).
//
//  Same principle as the Windows installer: add your own CD/ISO images, pick the
//  languages to install, click Install. The heavy lifting (ISO reading, edition
//  fingerprint, base + language overlays, voice dedup, CJK fonts) is done by the
//  shared portable core `vfimport`, invoked here as a subprocess.
//
//  No game data is shipped. Produces a `game_data` folder the standalone engine
//  boots (place it next to the Versailles AppImage, or install in place).
// ============================================================================
#include <QtWidgets>
#include <QProcess>

// One installable language, as reported by `vfimport plan`.
struct LangRow {
    QString code;       // fr, en, de, it, es, br, ja, ko, zh, or "?"
    bool complete=false;// false => missing its CD 1 (cannot be installed alone)
    QString cd1, cd2;   // CD edition sources (either may be empty)
    QString dvd, prefix;// DVD source + language folder
    QStringList buildArgs(bool asBase) const {
        QStringList a;
        a << (asBase ? "--base" : "--lang") << code;
        if (!dvd.isEmpty()) { a << "--prefix" << prefix << dvd; }
        else { if(!cd1.isEmpty()) a << cd1; if(!cd2.isEmpty()) a << cd2; }
        return a;
    }
};

static QString langName(const QString& c){
    static const QMap<QString,QString> N = {
        {"fr","Français"},{"en","English"},{"de","Deutsch"},{"it","Italiano"},
        {"es","Español"},{"br","Português"},{"ja","日本語"},{"ko","한국어"},{"zh","中文"}};
    return N.value(c, c);
}

class Installer : public QWidget {
public:
    Installer() {
        setWindowTitle("Versailles 1685 — Native Forge");
        resize(620, 560);

        vfimport = QCoreApplication::applicationDirPath() + "/vfimport";
        QString fdir = QCoreApplication::applicationDirPath() + "/fonts_cjk";
        if (QDir(fdir).exists()) fontsDir = fdir;

        auto *v = new QVBoxLayout(this);

        auto *intro = new QLabel(
            "<b>Édition native pour Windows moderne — portée sur Linux.</b><br>"
            "Ajoutez vos images CD/ISO de <i>Versailles 1685</i>, choisissez les langues, "
            "puis installez. Aucune donnée de jeu n'est fournie : tout provient de vos disques.");
        intro->setWordWrap(true);
        v->addWidget(intro);

        // Sources
        v->addWidget(new QLabel("<b>Sources (vos ISO)</b>"));
        srcList = new QListWidget;
        srcList->setMaximumHeight(110);
        v->addWidget(srcList);
        auto *hb = new QHBoxLayout;
        auto *addBtn = new QPushButton("Ajouter des ISO…");
        auto *clrBtn = new QPushButton("Vider");
        hb->addWidget(addBtn); hb->addWidget(clrBtn); hb->addStretch();
        v->addLayout(hb);

        // Languages (populated by scanning)
        v->addWidget(new QLabel("<b>Langues à installer</b> (la première cochée = langue de base)"));
        langWidget = new QListWidget;
        v->addWidget(langWidget, 1);

        // Target
        auto *tb = new QHBoxLayout;
        tb->addWidget(new QLabel("Dossier de destination :"));
        target = new QLineEdit(QDir::homePath() + "/Versailles1685/game_data");
        auto *browse = new QPushButton("Parcourir…");
        tb->addWidget(target, 1); tb->addWidget(browse);
        v->addLayout(tb);

        // Progress + actions
        progress = new QProgressBar; progress->setRange(0,100); progress->setValue(0);
        v->addWidget(progress);
        status = new QLabel(" "); status->setWordWrap(true);
        v->addWidget(status);
        auto *ab = new QHBoxLayout;
        installBtn = new QPushButton("Installer");
        installBtn->setEnabled(false);
        ab->addStretch(); ab->addWidget(installBtn);
        v->addLayout(ab);

        connect(addBtn, &QPushButton::clicked, this, &Installer::addIsos);
        connect(clrBtn, &QPushButton::clicked, this, [this]{ isoPaths.clear(); srcList->clear(); langWidget->clear(); installBtn->setEnabled(false); });
        connect(browse, &QPushButton::clicked, this, [this]{
            QString d = QFileDialog::getExistingDirectory(this, "Dossier de destination");
            if(!d.isEmpty()) target->setText(d + "/game_data");
        });
        connect(installBtn, &QPushButton::clicked, this, &Installer::doInstall);

        if (!QFileInfo::exists(vfimport))
            status->setText("⚠ Outil d'extraction 'vfimport' introuvable à côté de l'installateur.");
    }

private:
    QString vfimport, fontsDir;
    QStringList isoPaths;
    QListWidget *srcList, *langWidget;
    QLineEdit *target;
    QProgressBar *progress;
    QLabel *status;
    QPushButton *installBtn;

    void addIsos(){
        QStringList files = QFileDialog::getOpenFileNames(this, "Choisir des images ISO",
            QDir::homePath(), "Images disque (*.iso *.ISO);;Tous les fichiers (*)");
        for (const QString& f : files) if (!isoPaths.contains(f)) { isoPaths << f; srcList->addItem(QFileInfo(f).fileName()); }
        if (!isoPaths.isEmpty()) scan();
    }

    // Run `vfimport plan <isos...>` and populate the language checklist.
    void scan(){
        status->setText("Analyse des disques…");
        QCoreApplication::processEvents();
        QProcess p;
        QStringList args; args << "plan" << isoPaths;
        p.start(vfimport, args);
        if (!p.waitForFinished(120000)) { status->setText("⚠ Analyse trop longue."); return; }
        langWidget->clear();
        const QStringList lines = QString::fromUtf8(p.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        for (const QString& ln : lines) {
            const QStringList c = ln.split('\t');
            if (c.size() < 4 || c[0] != "LANG") continue;
            LangRow r; r.code = c[1]; r.complete = (c[2] == "1");
            for (const QString& kv : c[3].split(';')) {
                int eq = kv.indexOf('=');
                if (eq < 0) continue;
                QString k = kv.left(eq), val = kv.mid(eq+1);
                if (k=="cd1") r.cd1=val; else if (k=="cd2") r.cd2=val;
                else if (k=="dvd") r.dvd=val; else if (k=="prefix") r.prefix=val;
            }
            QString label = langName(r.code) + (r.dvd.isEmpty() ? "  (CD)" : "  (DVD)");
            if (!r.complete) label += "  — incomplet (CD 1 manquant)";
            auto *it = new QListWidgetItem(label, langWidget);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Unchecked);
            if (!r.complete) it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
            it->setData(Qt::UserRole, QVariant::fromValue(rowToMap(r)));
        }
        int usable = 0;
        for (int i=0;i<langWidget->count();i++) if (langWidget->item(i)->flags() & Qt::ItemIsEnabled) usable++;
        status->setText(usable ? QString("%1 langue(s) trouvée(s). Cochez-en au moins une.").arg(usable)
                               : "Aucune édition complète trouvée (ajoutez le CD 1 et le CD 2).");
        installBtn->setEnabled(usable > 0);
    }

    static QVariantMap rowToMap(const LangRow& r){
        QVariantMap m; m["code"]=r.code; m["complete"]=r.complete;
        m["cd1"]=r.cd1; m["cd2"]=r.cd2; m["dvd"]=r.dvd; m["prefix"]=r.prefix; return m;
    }
    static LangRow mapToRow(const QVariantMap& m){
        LangRow r; r.code=m["code"].toString(); r.complete=m["complete"].toBool();
        r.cd1=m["cd1"].toString(); r.cd2=m["cd2"].toString();
        r.dvd=m["dvd"].toString(); r.prefix=m["prefix"].toString(); return r;
    }

    void doInstall(){
        QList<LangRow> chosen;
        for (int i=0;i<langWidget->count();i++){
            auto *it = langWidget->item(i);
            if (it->checkState()==Qt::Checked) chosen << mapToRow(it->data(Qt::UserRole).toMap());
        }
        if (chosen.isEmpty()) { status->setText("Cochez au moins une langue."); return; }

        QStringList args; args << "build" << target->text();
        if (!fontsDir.isEmpty()) args << "--fonts" << fontsDir;
        for (int i=0;i<chosen.size();i++) args << chosen[i].buildArgs(i==0);

        installBtn->setEnabled(false);
        progress->setRange(0,0);          // busy indicator (extraction has no cheap total)
        status->setText("Installation en cours… (extraction des données, cela peut prendre quelques minutes)");

        auto *proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, this, [this,proc]{
            QString last = QString::fromUtf8(proc->readAll()).trimmed().split('\n').last();
            if(!last.isEmpty()) status->setText(last);
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this,proc](int code, QProcess::ExitStatus){
                progress->setRange(0,100);
                if (code==0){ progress->setValue(100); status->setText("✅ Installation terminée. Copiez le dossier game_data à côté de l'AppImage du jeu."); }
                else { progress->setValue(0); status->setText("⚠ Échec de l'installation (code "+QString::number(code)+")."); installBtn->setEnabled(true); }
                proc->deleteLater();
            });
        proc->start(vfimport, args);
    }
};

int main(int argc, char** argv){
    QApplication app(argc, argv);
    Installer w; w.show();
    return app.exec();
}
