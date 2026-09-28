package org.scummvm.scummvm;

// ============================================================================
//  Native Forge — first-run ISO import for the standalone Versailles app.
//
//  This is the app's launcher. If the game data has already been imported it
//  jumps straight into the game. Otherwise it lets the player pick their own
//  CD/ISO image(s), runs the bundled `vfimport` core (libvfimport.so) to
//  extract a `game_data` folder into the app's storage, then launches the game
//  (ScummVM auto-detects and boots it via --path/--auto-detect).
//
//  No game data is shipped: everything comes from the user's own disc images.
//
//  NOTE: the on-device runtime (all-files access, exec of the bundled binary,
//  launch of the imported game) needs testing on a real device; the import
//  logic and the vfimport core are validated separately.
// ============================================================================

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

public class ImportActivity extends Activity {

	private static final int REQ_PICK_ISOS = 4101;

	private File gameDataDir;
	private String vfimport;
	private final List<String> isoPaths = new ArrayList<>();

	// One installable language reported by `vfimport plan`.
	private static class LangRow {
		String code; boolean complete;
		String cd1 = "", cd2 = "", dvd = "", prefix = "";
		CheckBox box;
	}
	private final List<LangRow> langs = new ArrayList<>();

	private LinearLayout langBox;
	private TextView status;
	private Button installBtn;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		gameDataDir = new File(getExternalFilesDir(null), "game_data");
		vfimport = new File(getApplicationInfo().nativeLibraryDir, "libvfimport.so").getAbsolutePath();

		// Already imported? Go straight to the game.
		if (gameDataReady()) { launchGame(); return; }

		buildUi();
	}

	private boolean gameDataReady() {
		File datas = new File(gameDataDir, "DATAS_V");
		return datas.isDirectory();
	}

	private void buildUi() {
		LinearLayout root = new LinearLayout(this);
		root.setOrientation(LinearLayout.VERTICAL);
		int pad = dp(16);
		root.setPadding(pad, pad, pad, pad);

		TextView title = new TextView(this);
		title.setText("Versailles 1685 — installation");
		title.setTextSize(20);
		root.addView(title);

		TextView intro = new TextView(this);
		intro.setText("Choisissez vos images CD/ISO de Versailles 1685, puis installez. "
			+ "Aucune donnée n'est fournie : tout provient de vos propres disques.");
		root.addView(intro);

		Button pick = new Button(this);
		pick.setText("Choisir des ISO…");
		pick.setOnClickListener(v -> pickIsos());
		root.addView(pick);

		TextView langTitle = new TextView(this);
		langTitle.setText("Langues à installer (la première cochée = langue de base) :");
		root.addView(langTitle);

		langBox = new LinearLayout(this);
		langBox.setOrientation(LinearLayout.VERTICAL);
		root.addView(langBox);

		installBtn = new Button(this);
		installBtn.setText("Installer");
		installBtn.setEnabled(false);
		installBtn.setOnClickListener(v -> doInstall());
		root.addView(installBtn);

		status = new TextView(this);
		status.setText("");
		root.addView(status);

		ScrollView sv = new ScrollView(this);
		sv.addView(root);
		setContentView(sv);
	}

	private int dp(int v) { return (int) (v * getResources().getDisplayMetrics().density); }

	// -- ISO selection -------------------------------------------------------
	private void pickIsos() {
		if (!ensureAllFilesAccess()) return;
		Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
		i.addCategory(Intent.CATEGORY_OPENABLE);
		i.setType("*/*");
		i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
		startActivityForResult(Intent.createChooser(i, "Choisir des ISO"), REQ_PICK_ISOS);
	}

	// vfimport reads the ISO by file PATH, so we need all-files access on Android 11+.
	private boolean ensureAllFilesAccess() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			if (!Environment.isExternalStorageManager()) {
				try {
					Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
						Uri.parse("package:" + getPackageName()));
					startActivity(i);
				} catch (Exception e) {
					startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
				}
				toast("Autorisez l'accès à tous les fichiers, puis rechoisissez vos ISO.");
				return false;
			}
		}
		return true;
	}

	@Override
	protected void onActivityResult(int req, int res, Intent data) {
		super.onActivityResult(req, res, data);
		if (req != REQ_PICK_ISOS || res != RESULT_OK || data == null) return;
		isoPaths.clear();
		if (data.getClipData() != null) {
			for (int k = 0; k < data.getClipData().getItemCount(); k++)
				addIso(data.getClipData().getItemAt(k).getUri());
		} else if (data.getData() != null) {
			addIso(data.getData());
		}
		if (isoPaths.isEmpty()) { toast("Impossible de résoudre le chemin des ISO choisies."); return; }
		runPlan();
	}

	private void addIso(Uri uri) {
		String p = uriToPath(uri);
		if (p != null && new File(p).exists() && !isoPaths.contains(p)) isoPaths.add(p);
	}

	// Resolve a primary-storage document URI to a real filesystem path.
	private String uriToPath(Uri uri) {
		try {
			String id = DocumentsContract.getDocumentId(uri);
			String[] parts = id.split(":", 2);
			if (parts.length == 2 && "primary".equalsIgnoreCase(parts[0]))
				return Environment.getExternalStorageDirectory() + "/" + parts[1];
		} catch (Exception ignored) {}
		return null;
	}

	// -- Planning (group discs into languages) -------------------------------
	private void runPlan() {
		status.setText("Analyse des disques…");
		langs.clear();
		langBox.removeAllViews();
		new Thread(() -> {
			List<String> args = new ArrayList<>();
			args.add("plan");
			args.addAll(isoPaths);
			String out = runVfimport(args);
			final List<LangRow> found = parsePlan(out);
			ui(() -> showLangs(found));
		}).start();
	}

	private List<LangRow> parsePlan(String out) {
		List<LangRow> rows = new ArrayList<>();
		if (out == null) return rows;
		for (String ln : out.split("\n")) {
			String[] c = ln.split("\t");
			if (c.length < 4 || !c[0].equals("LANG")) continue;
			LangRow r = new LangRow();
			r.code = c[1]; r.complete = c[2].equals("1");
			for (String kv : c[3].split(";")) {
				int e = kv.indexOf('=');
				if (e < 0) continue;
				String k = kv.substring(0, e), val = kv.substring(e + 1);
				switch (k) {
					case "cd1": r.cd1 = val; break;
					case "cd2": r.cd2 = val; break;
					case "dvd": r.dvd = val; break;
					case "prefix": r.prefix = val; break;
				}
			}
			rows.add(r);
		}
		return rows;
	}

	private void showLangs(List<LangRow> found) {
		langs.clear(); langBox.removeAllViews();
		int usable = 0;
		for (LangRow r : found) {
			CheckBox cb = new CheckBox(this);
			String label = langName(r.code) + (r.dvd.isEmpty() ? " (CD)" : " (DVD)");
			if (!r.complete) { label += " — incomplet (CD 1 manquant)"; cb.setEnabled(false); }
			cb.setText(label);
			r.box = cb;
			langs.add(r);
			langBox.addView(cb);
			if (r.complete) usable++;
		}
		installBtn.setEnabled(usable > 0);
		status.setText(usable > 0 ? (usable + " langue(s) trouvée(s). Cochez-en au moins une.")
			: "Aucune édition complète (ajoutez le CD 1 et le CD 2).");
	}

	// -- Install -------------------------------------------------------------
	private void doInstall() {
		List<LangRow> chosen = new ArrayList<>();
		for (LangRow r : langs) if (r.box != null && r.box.isChecked()) chosen.add(r);
		if (chosen.isEmpty()) { toast("Cochez au moins une langue."); return; }

		installBtn.setEnabled(false);
		status.setText("Installation en cours… (extraction, cela peut prendre quelques minutes)");

		final List<String> args = new ArrayList<>();
		args.add("build");
		args.add(gameDataDir.getAbsolutePath());
		File fonts = new File(getExternalFilesDir(null), "fonts_cjk");
		if (fonts.isDirectory()) { args.add("--fonts"); args.add(fonts.getAbsolutePath()); }
		for (int i = 0; i < chosen.size(); i++) {
			LangRow r = chosen.get(i);
			args.add(i == 0 ? "--base" : "--lang");
			args.add(r.code);
			if (!r.dvd.isEmpty()) { args.add("--prefix"); args.add(r.prefix); args.add(r.dvd); }
			else { if (!r.cd1.isEmpty()) args.add(r.cd1); if (!r.cd2.isEmpty()) args.add(r.cd2); }
		}

		gameDataDir.mkdirs();
		new Thread(() -> {
			String out = runVfimport(args);
			final boolean ok = gameDataReady();
			ui(() -> {
				if (ok) { status.setText("✅ Terminé. Lancement du jeu…"); launchGame(); }
				else { status.setText("⚠ Échec de l'installation.\n" + (out == null ? "" : tail(out)));
					installBtn.setEnabled(true); }
			});
		}).start();
	}

	// -- vfimport subprocess -------------------------------------------------
	private String runVfimport(List<String> args) {
		List<String> cmd = new ArrayList<>();
		cmd.add(vfimport);
		cmd.addAll(args);
		try {
			ProcessBuilder pb = new ProcessBuilder(cmd);
			pb.redirectErrorStream(true);
			Process p = pb.start();
			StringBuilder sb = new StringBuilder();
			BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
			String line;
			while ((line = br.readLine()) != null) sb.append(line).append('\n');
			p.waitFor();
			return sb.toString();
		} catch (Exception e) {
			return null;
		}
	}

	// -- Launch the game -----------------------------------------------------
	private void launchGame() {
		startActivity(new Intent(this, SplashActivity.class));
		finish();
	}

	// -- helpers -------------------------------------------------------------
	private void ui(Runnable r) { new Handler(Looper.getMainLooper()).post(r); }
	private void toast(String s) { Toast.makeText(this, s, Toast.LENGTH_LONG).show(); }
	private static String tail(String s) { int n = s.length(); return n > 400 ? s.substring(n - 400) : s; }

	private static String langName(String c) {
		switch (c) {
			case "fr": return "Français";
			case "en": return "English";
			case "de": return "Deutsch";
			case "it": return "Italiano";
			case "es": return "Español";
			case "br": return "Português";
			case "ja": return "日本語";
			case "ko": return "한국어";
			case "zh": return "中文";
			default: return c;
		}
	}
}
