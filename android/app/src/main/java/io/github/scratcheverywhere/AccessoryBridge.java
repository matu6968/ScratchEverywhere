package io.github.scratcheverywhere;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.util.Log;

/**
 * The Java half of the Android Open Accessory link. Only Java can ask for permission and open an accessory; what comes
 * out of that is a file descriptor, which is handed to the native side (see accessory.hpp) that does the actual I/O.
 */
public final class AccessoryBridge {
	private static final String TAG = "AccessoryBridge";
	private static final String ACTION_PERMISSION = "io.github.scratcheverywhere.USB_ACCESSORY_PERMISSION";

	private static native void nativeAttached(int fd, String manufacturer, String model, String description, String version, String uri, String serial);

	private static native void nativeDetached();

	private static Activity activity;
	private static UsbManager usbManager;
	private static UsbAccessory opened; // The accessory we gave to the native side, if any
	private static boolean askingPermission;
	private static boolean nativeAvailable = true;

	private static final BroadcastReceiver receiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			// An exception in here would take the whole app down
			try {
				String action = intent.getAction();
				UsbAccessory accessory = intent.getParcelableExtra(UsbManager.EXTRA_ACCESSORY);

				if (ACTION_PERMISSION.equals(action)) {
					askingPermission = false;
					if (accessory != null && intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
						open(accessory);
					} else {
						Log.w(TAG, "Permission to use the accessory was denied");
					}
				} else if (UsbManager.ACTION_USB_ACCESSORY_DETACHED.equals(action)) {
					// Android has a single accessory at a time, so this is ours
					askingPermission = false;
					close();
				}
			} catch (RuntimeException e) {
				Log.e(TAG, "Failed to handle " + intent.getAction(), e);
			}
		}
	};

	private AccessoryBridge() {
	}

	/** Starts listening for accessories and picks up one that is already attached. Call after the native libraries are loaded. */
	public static synchronized void init(Activity a) {
		if (activity != null) return;

		// Without accessory support (the phone's kernel or ROM lacks it) the system has no USB service for us to talk to,
		// and its UsbManager fails with a NullPointerException. Not having AOA is fine, the app just can't use it.
		if (!a.getPackageManager().hasSystemFeature("android.hardware.usb.accessory")) {
			Log.i(TAG, "This device doesn't support USB accessories");
			return;
		}

		try {
			UsbManager manager = (UsbManager) a.getSystemService(Context.USB_SERVICE);
			if (manager == null) return;

			IntentFilter filter = new IntentFilter();
			filter.addAction(ACTION_PERMISSION);
			filter.addAction(UsbManager.ACTION_USB_ACCESSORY_DETACHED);
			if (Build.VERSION.SDK_INT >= 33) {
				a.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
			} else {
				a.registerReceiver(receiver, filter);
			}

			activity = a;
			usbManager = manager;
			checkAttached();
		} catch (RuntimeException e) {
			Log.w(TAG, "USB accessories are not available", e);
		}
	}

	public static synchronized void release() {
		if (activity == null) return;
		try {
			activity.unregisterReceiver(receiver);
		} catch (IllegalArgumentException ignored) {
		}
		close();
		activity = null;
		usbManager = null;
	}

	/** For the activity's onNewIntent(): the system starts the app with an intent when an accessory it likes is plugged in. */
	public static synchronized void handleIntent(Intent intent) {
		if (activity == null || intent == null) return;
		if (!UsbManager.ACTION_USB_ACCESSORY_ATTACHED.equals(intent.getAction())) return;

		try {
			UsbAccessory accessory = intent.getParcelableExtra(UsbManager.EXTRA_ACCESSORY);
			if (accessory != null) use(accessory);
			else checkAttached();
		} catch (RuntimeException e) {
			Log.w(TAG, "Failed to use the accessory", e);
		}
	}

	private static boolean same(String a, String b) {
		return a == null ? b == null : a.equals(b);
	}

	/**
	 * UsbAccessory.equals() reads the serial number, which throws a SecurityException as soon as the permission is gone
	 * (for instance when the accessory has just been unplugged), so compare the other fields instead.
	 */
	private static boolean same(UsbAccessory a, UsbAccessory b) {
		if (a == null || b == null) return false;
		return same(a.getManufacturer(), b.getManufacturer()) && same(a.getModel(), b.getModel()) && same(a.getDescription(), b.getDescription()) && same(a.getVersion(), b.getVersion()) && same(a.getUri(), b.getUri());
	}

	private static void checkAttached() {
		UsbAccessory[] accessories = usbManager.getAccessoryList();
		// Android only ever has a single accessory attached
		if (accessories != null && accessories.length > 0) use(accessories[0]);
	}

	private static void use(UsbAccessory accessory) {
		if (same(accessory, opened)) return;

		if (usbManager.hasPermission(accessory)) {
			open(accessory);
			return;
		}

		if (askingPermission) return;
		askingPermission = true;

		Intent intent = new Intent(ACTION_PERMISSION);
		intent.setPackage(activity.getPackageName());
		// The system fills in the result, so the intent has to be mutable on the newer versions that care
		int flags = Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0;
		usbManager.requestPermission(accessory, PendingIntent.getBroadcast(activity, 0, intent, flags));
	}

	private static void open(UsbAccessory accessory) {
		if (same(accessory, opened) || !nativeAvailable) return;

		ParcelFileDescriptor descriptor;
		try {
			descriptor = usbManager.openAccessory(accessory);
		} catch (SecurityException e) {
			Log.w(TAG, "No permission to open the accessory", e);
			return;
		}
		if (descriptor == null) {
			Log.e(TAG, "Could not open the accessory");
			return;
		}

		try {
			// The native side owns the descriptor from here on and closes it when it is done with it
			nativeAttached(descriptor.detachFd(), accessory.getManufacturer(), accessory.getModel(), accessory.getDescription(), accessory.getVersion(), accessory.getUri(), accessory.getSerial());
			opened = accessory;
		} catch (UnsatisfiedLinkError e) {
			Log.e(TAG, "The native library doesn't have the accessory link", e);
			nativeAvailable = false;
		}
	}

	private static void close() {
		if (opened == null) return;
		opened = null;
		try {
			nativeDetached();
		} catch (UnsatisfiedLinkError ignored) {
		}
	}
}
