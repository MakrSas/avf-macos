package com.avfmacos.vmdisplay;

import android.app.Activity;
import android.os.Bundle;
import android.system.virtualmachine.VirtualMachine;
import android.system.virtualmachine.VirtualMachineCallback;
import android.system.virtualmachine.VirtualMachineConfig;
import android.system.virtualmachine.VirtualMachineCustomImageConfig;
import android.system.virtualmachine.VirtualMachineException;
import android.system.virtualmachine.VirtualMachineManager;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;
import android.widget.FrameLayout;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Minimal AVF custom-VM host: creates a SurfaceView, wires it to a custom
 * bootloader/disk VM via the public android.system.virtualmachine API, and
 * requests GPU/display -- the piece the bare `vm run` CLI cannot provide
 * (confirmed: crosvm only gets --android-display-service when a real app
 * process with a Surface hosts the VM via virtmgr; see docs/AVF_HARDWARE.md
 * in the project repo).
 *
 * Paths below point at /data/local/tmp/avf-macos, matching where the CI-built
 * bootloader.bin / EDK2 firmware / disk images get pushed by this project's
 * existing adb workflow.
 */
public class MainActivity extends Activity {
    private static final String TAG = "vmdisplay";

    private static final String BOOTLOADER_PATH =
            "/data/local/tmp/avf-macos/QEMU_EFI.fd";
    private static final String DISK_PATH =
            "/data/local/tmp/avf-macos/win11_disk.img";
    private static final String ISO_PATH =
            "/data/local/tmp/avf-macos/Win11.iso";

    private VirtualMachine vm;
    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FrameLayout root = new FrameLayout(this);
        SurfaceView surfaceView = new SurfaceView(this);
        status = new TextView(this);
        status.setText("starting...");
        root.addView(surfaceView);
        root.addView(status);
        setContentView(root);

        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                setStatus("surface ready, starting VM");
                startVm();
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                stopVm();
            }
        });
    }

    private void setStatus(String s) {
        Log.i(TAG, s);
        runOnUiThread(() -> status.setText(s));
    }

    private void dumpApi(Class<?> cls) {
        StringBuilder sb = new StringBuilder();
        sb.append("=== ").append(cls.getName()).append(" ===\n");
        for (java.lang.reflect.Constructor<?> c : cls.getDeclaredConstructors()) {
            sb.append("  ctor: ").append(c).append("\n");
        }
        for (java.lang.reflect.Method m : cls.getDeclaredMethods()) {
            sb.append("  method: ").append(m).append("\n");
        }
        for (Class<?> inner : cls.getDeclaredClasses()) {
            sb.append("  inner: ").append(inner.getName()).append("\n");
        }
        Log.i(TAG, sb.toString());
    }

    private void dumpAllApis() {
        try {
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineCustomImageConfig"));
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineCustomImageConfig$Builder"));
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineCustomImageConfig$DisplayConfig"));
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineCustomImageConfig$DisplayConfig$Builder"));
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineCustomImageConfig$GpuConfig"));
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineCustomImageConfig$GpuConfig$Builder"));
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineConfig"));
            dumpApi(Class.forName("android.system.virtualmachine.VirtualMachineConfig$Builder"));
        } catch (Throwable t) {
            Log.e(TAG, "dumpAllApis failed", t);
        }
    }

    private void startVm() {
        dumpAllApis();
        try {
            VirtualMachineManager vmm = getSystemService(VirtualMachineManager.class);
            if (vmm == null) {
                setStatus("AVF not supported on this device/build");
                return;
            }

            VirtualMachineCustomImageConfig.DisplayConfig displayConfig =
                    new VirtualMachineCustomImageConfig.DisplayConfig.Builder()
                            .setWidth(1280)
                            .setHeight(720)
                            .setHorizontalDpi(160)
                            .setVerticalDpi(160)
                            .setRefreshRate(60)
                            .build();

            // backend="2d" (not "virglrenderer"/virgl2) matches the exact
            // crosvm --gpu flags of a confirmed-working graphical VM (the
            // stock Debian Terminal app) on this device. virglrenderer
            // crashed crosvm's GPU worker thread with "invalid rutabaga
            // build parameters" -- see docs/PROGRESS.md.
            VirtualMachineCustomImageConfig.GpuConfig gpuConfig =
                    new VirtualMachineCustomImageConfig.GpuConfig.Builder()
                            .setBackend("2d")
                            .setRendererUseEgl(true)
                            .setRendererUseGles(true)
                            .setRendererUseSurfaceless(true)
                            .build();

            VirtualMachineCustomImageConfig.Builder customBuilder =
                    new VirtualMachineCustomImageConfig.Builder()
                            .setName("avf-macos-windows")
                            .setBootloaderPath(BOOTLOADER_PATH)
                            .setDisplayConfig(displayConfig)
                            .setGpuConfig(gpuConfig)
                            .useTouch(true)
                            .useKeyboard(true)
                            .useMouse(true)
                            .useAutoMemoryBalloon(false);

            VirtualMachineCustomImageConfig customImageConfig = customBuilder.build();

            VirtualMachineConfig config =
                    new VirtualMachineConfig.Builder(this)
                            .setCustomImageConfig(customImageConfig)
                            .setMemoryBytes(4096L * 1024 * 1024)
                            .setProtectedVm(false)
                            .build();

            vm = vmm.getOrCreate("avf-macos-windows", config);
            vm.setCallback(getMainExecutor(), new VirtualMachineCallback() {
                @Override
                public void onPayloadStarted(VirtualMachine vm) {
                    setStatus("payload started");
                }

                @Override
                public void onPayloadReady(VirtualMachine vm) {
                    setStatus("payload ready");
                }

                @Override
                public void onPayloadFinished(VirtualMachine vm, int exitCode) {
                    setStatus("payload finished, exit=" + exitCode);
                }

                @Override
                public void onError(VirtualMachine vm, int errorCode, String message) {
                    setStatus("error " + errorCode + ": " + message);
                }

                @Override
                public void onStopped(VirtualMachine vm, int reason) {
                    setStatus("stopped, reason=" + reason);
                }
            });

            vm.run();
            setStatus("vm.run() called");
        } catch (VirtualMachineException e) {
            Log.e(TAG, "failed to start VM", e);
            setStatus("failed: " + e);
        } catch (Exception e) {
            Log.e(TAG, "unexpected error", e);
            setStatus("unexpected error: " + e);
        }
    }

    private void stopVm() {
        if (vm != null) {
            try {
                vm.stop();
            } catch (Exception e) {
                Log.e(TAG, "error stopping VM", e);
            }
        }
    }

    @Override
    protected void onDestroy() {
        stopVm();
        super.onDestroy();
    }
}
