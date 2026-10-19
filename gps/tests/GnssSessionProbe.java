// SPDX-License-Identifier: Apache-2.0
// Optional root app_process test: drops to UID 1000 and never prints coordinates.
import android.app.ActivityThread;
import android.content.Context;
import android.location.GnssStatus;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

public final class GnssSessionProbe {
    public static void main(String[] args) {
        try {
            run(args);
        } catch (Exception error) {
            error.printStackTrace();
            System.exit(1);
        }
    }
    private static void run(String[] args) throws Exception {
        // LocationManager validates that package "android" belongs to the calling UID.
        if (Process.myUid() == 0) {
            if (Process.setGid(Process.SYSTEM_UID) != 0 || Process.setUid(Process.SYSTEM_UID) != 0) {
                throw new IllegalStateException("Cannot switch test process to system UID");
            }
        }
        if (Process.myUid() != Process.SYSTEM_UID) {
            throw new IllegalStateException("Start the probe from a root ADB shell");
        }
        Looper.prepareMainLooper();
        Context context = ActivityThread.systemMain().getSystemContext();
        LocationManager manager = context.getSystemService(LocationManager.class);
        if (!manager.isLocationEnabled()) throw new IllegalStateException("Location is disabled");
        ExecutorService executor = Executors.newSingleThreadExecutor();
        AtomicInteger reports = new AtomicInteger(), fixes = new AtomicInteger();
        long started = SystemClock.elapsedRealtime();
        GnssStatus.Callback status = new GnssStatus.Callback() {
            @Override public void onStarted() { System.out.println("GNSS engine started"); }
            @Override public void onFirstFix(int ttff) { System.out.println("TTFF=" + ttff + " ms"); }
            @Override public void onSatelliteStatusChanged(GnssStatus sv) {
                reports.incrementAndGet();
                int[] count = new int[8], used = new int[8];
                float[] strongest = new float[8];
                for (int i = 0; i < sv.getSatelliteCount(); ++i) {
                    int type = sv.getConstellationType(i);
                    if (type < 0 || type >= count.length) continue;
                    count[type]++;
                    if (sv.usedInFix(i)) used[type]++;
                    strongest[type] = Math.max(strongest[type], sv.getCn0DbHz(i));
                }
                String line = "SV reports=" + reports.get() + " total=" + sv.getSatelliteCount();
                for (int type = 1; type < count.length; ++type) {
                    if (count[type] != 0) line += " constellation=" + type + " count=" + count[type]
                            + " used=" + used[type] + " maxCn0=" + strongest[type];
                }
                System.out.println(line);
            }
        };
        LocationListener listener = location -> {
            fixes.incrementAndGet();
            System.out.println("FIX elapsed=" + (SystemClock.elapsedRealtime() - started)
                    + " ms accuracy=" + location.getAccuracy() + " m");
        };
        boolean registered = false;
        try {
            if (!manager.registerGnssStatusCallback(executor, status)) {
                throw new IllegalStateException("GNSS callback registration failed");
            }
            registered = true;
            manager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000, 0, executor, listener);
            if (args.length == 1 && args[0].equals("--assist")) {
                System.out.println("PSDS request=" + manager.sendExtraCommand(
                        LocationManager.GPS_PROVIDER, "force_psds_injection", null));
                System.out.println("Time request=" + manager.sendExtraCommand(
                        LocationManager.GPS_PROVIDER, "force_time_injection", null));
            }
            Thread.sleep(30000);
        } finally {
            manager.removeUpdates(listener);
            if (registered) manager.unregisterGnssStatusCallback(status);
            executor.shutdownNow();
        }
        System.out.println("SESSION COMPLETED svReports=" + reports.get() + " fixes=" + fixes.get()
                + "; absence of a fix indoors is not an RF comparison");
        System.exit(0);
    }
}
