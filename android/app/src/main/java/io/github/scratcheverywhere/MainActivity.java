package io.github.scratcheverywhere;

import android.content.Intent;
import android.os.Bundle;
import org.libsdl.app.SDLActivity;
import org.woheller69.freeDroidWarn.FreeDroidWarn;

public class MainActivity extends SDLActivity {
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		FreeDroidWarn.showWarningOnUpgrade(this, BuildConfig.VERSION_CODE);
		AccessoryBridge.init(this); // The native libraries have been loaded by now
	}

	@Override
	protected void onNewIntent(Intent intent) {
		super.onNewIntent(intent);
		setIntent(intent);
		AccessoryBridge.handleIntent(intent);
	}

	@Override
	protected void onDestroy() {
		AccessoryBridge.release();
		super.onDestroy();
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"SDL2",     // Has to be shared, SDLActivity's JNI functions live in here
			"scratch"   // Scratch Everywhere!
		};
	}
}
