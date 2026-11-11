package io.github.scratcheverywhere;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipFile;

import org.json.JSONObject;

/**
 * Copies .sb3 projects handed to the app ("Open with", "Share") into the folder the
 * native side scans for projects, then goes on to the main activity.
 */
public class ProjectImportActivity extends Activity {
	private static final String TAG = "ProjectImportActivity";

	private final List<Uri> pending = new ArrayList<>();
	private int successful = 0;
	private int failed = 0;
	private int skipped = 0; // The user chose not to overwrite an existing project
	private int notProjects = 0; // Files that turned out not to be Scratch projects
	private volatile boolean lastNotProject = false;
	private int total = 0;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		handleIntent(getIntent());
	}

	@Override
	protected void onNewIntent(Intent intent) {
		super.onNewIntent(intent);
		setIntent(intent);
		handleIntent(intent);
	}

	private void handleIntent(Intent intent) {
		pending.clear();
		successful = 0;
		failed = 0;
		skipped = 0;
		notProjects = 0;

		String action = intent.getAction();
		if (Intent.ACTION_VIEW.equals(action)) {
			Uri uri = intent.getData();
			if (uri != null) pending.add(uri);
		} else if (Intent.ACTION_SEND.equals(action)) {
			Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
			if (uri != null) pending.add(uri);
		} else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
			ArrayList<Uri> uris = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
			if (uris != null) pending.addAll(uris);
		}

		total = pending.size();
		if (total == 0) {
			returnToMain();
			return;
		}
		importNext();
	}

	/** Imports the queued URIs one at a time, since overwriting needs a confirmation dialog. */
	private void importNext() {
		if (pending.isEmpty()) {
			finishImport();
			return;
		}

		final Uri uri = pending.remove(0);
		final String fileName = getFileName(uri);
		final File dest = new File(getScratchFolder(), fileName);
		Log.i(TAG, "Importing " + uri + " as " + dest);

		if (!dest.exists()) {
			copyInBackground(uri, dest);
			return;
		}

		new AlertDialog.Builder(this)
			.setMessage(getString(R.string.import_exists))
			.setNegativeButton(android.R.string.cancel, (dialog, which) -> {
				skipped++;
				importNext();
			})
			.setOnCancelListener(dialog -> {
				// Back button or tapping outside the dialog
				skipped++;
				importNext();
			})
			.setPositiveButton(android.R.string.ok, (dialog, which) -> copyInBackground(uri, dest))
			.setCancelable(true)
			.show();
	}

	private void copyInBackground(final Uri uri, final File dest) {
		new Thread(() -> {
			final boolean ok = copy(uri, dest);
			runOnUiThread(() -> {
				if (ok) successful++;
				else if (lastNotProject) notProjects++;
				else failed++;
				importNext();
			});
		}).start();
	}

	private void finishImport() {
		if (successful == 0 && failed == 0 && notProjects == 0) {
			// Everything was declined, so there is nothing to report or to open: go back to where the user came from
			finish();
			return;
		}

		if (total == 1) {
			int message = successful == 1 ? R.string.import_success : (notProjects == 1 ? R.string.import_not_project : R.string.import_failed);
			Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
		} else {
			Toast.makeText(this, String.format(getString(R.string.import_bulk), successful, failed + notProjects), Toast.LENGTH_LONG).show();
		}

		// A file that isn't a project (we are offered all sorts of files) shouldn't drag the user into the app
		if (successful == 0) finish();
		else returnToMain();
	}

	private boolean copy(Uri uri, File dest) {
		lastNotProject = false;
		File temp = new File(dest.getParentFile(), dest.getName() + ".part");
		InputStream input = null;
		OutputStream output = null;
		try {
			input = getContentResolver().openInputStream(uri);
			if (input == null) throw new IOException("No stream for " + uri);
			output = new FileOutputStream(temp);

			byte[] buf = new byte[8192];
			int len;
			while ((len = input.read(buf)) > 0) {
				output.write(buf, 0, len);
			}
			output.close();
			output = null;

			if (!isScratchProject(temp)) {
				Log.w(TAG, uri + " is not a Scratch project");
				lastNotProject = true;
				temp.delete();
				return false;
			}

			// Only replace an existing project once the new one is known to be complete and valid
			if (dest.exists() && !dest.delete()) throw new IOException("Could not replace " + dest);
			if (!temp.renameTo(dest)) throw new IOException("Could not move " + temp + " to " + dest);
			return true;
		} catch (Exception e) {
			Log.e(TAG, "Failed to import " + uri, e);
			// Don't leave a truncated project behind
			temp.delete();
			return false;
		} finally {
			try {
				if (input != null) input.close();
				if (output != null) output.close();
			} catch (IOException ignored) {
			}
		}
	}

	/** .sb3 and .sb2 projects are zip archives with a project.json in them. */
	private static boolean isScratchProject(File file) {
		ZipFile zip = null;
		try {
			zip = new ZipFile(file);
			return zip.getEntry("project.json") != null;
		} catch (Exception e) {
			return false;
		} finally {
			try {
				if (zip != null) zip.close();
			} catch (IOException ignored) {
			}
		}
	}

	/** A file name that is safe to use inside the projects folder. */
	private String getFileName(Uri uri) {
		String name = null;

		Cursor cursor = null;
		try {
			cursor = getContentResolver().query(uri, null, null, null, null);
			if (cursor != null && cursor.moveToFirst()) {
				int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
				if (nameIndex >= 0) name = cursor.getString(nameIndex);
			}
		} catch (Exception e) {
			Log.w(TAG, "Could not query the display name of " + uri, e);
		} finally {
			if (cursor != null) cursor.close();
		}

		if (name == null || name.isEmpty()) name = uri.getLastPathSegment();
		if (name == null || name.isEmpty()) name = "project";

		// Keep path separators out of the name so it can't escape the projects folder
		name = name.substring(name.lastIndexOf('/') + 1).replace('\\', '_');
		if (name.isEmpty() || name.equals(".") || name.equals("..")) name = "project";
		if (!name.toLowerCase().contains(".sb")) name += ".sb3";
		return name;
	}

	/**
	 * Where the native side looks for projects: the custom projects path from Settings.json if
	 * it is enabled, the app's external files folder otherwise (same as OS::getScratchFolderLocation).
	 */
	private File getScratchFolder() {
		File base = getExternalFilesDir(null);
		if (base == null) base = getFilesDir();

		File settings = new File(base, "Settings.json");
		if (settings.isFile()) {
			BufferedReader reader = null;
			try {
				reader = new BufferedReader(new InputStreamReader(new FileInputStream(settings), "UTF-8"));
				StringBuilder content = new StringBuilder();
				String line;
				while ((line = reader.readLine()) != null) content.append(line).append('\n');

				JSONObject json = new JSONObject(content.toString());
				String custom = json.optString("ProjectsPath", "");
				if (json.optBoolean("UseProjectsPath", false) && !custom.isEmpty()) {
					File dir = new File(custom);
					if (dir.isDirectory() || dir.mkdirs()) return dir;
				}
			} catch (Exception e) {
				Log.w(TAG, "Could not read the custom projects path from Settings.json", e);
			} finally {
				try {
					if (reader != null) reader.close();
				} catch (IOException ignored) {
				}
			}
		}

		return base;
	}

	private void returnToMain() {
		startActivity(new Intent(this, MainActivity.class));
		finish();
	}
}
