package io.github.scratcheverywhere;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import org.libsdl.app.SDLActivity;
import org.woheller69.freeDroidWarn.FreeDroidWarn;

public class MainActivity extends SDLActivity {
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWritePermission();
		FreeDroidWarn.showWarningOnUpgrade(this, BuildConfig.VERSION_CODE);
	}

	private void requestWritePermission() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
			if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
					!= PackageManager.PERMISSION_GRANTED) {
				ActivityCompat.requestPermissions(this,
						new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
						1);
			}
		}
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"scratch"   // Scratch Everywhere!
		};
	}
}
