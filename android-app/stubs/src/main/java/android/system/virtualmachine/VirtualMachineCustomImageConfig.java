package android.system.virtualmachine;

public class VirtualMachineCustomImageConfig {

    public static final class Disk {
        public static final class Builder {
            public Disk build() { return new Disk(); }
        }
    }

    public static final class SharedPath {
        public static final class Builder {
            public SharedPath build() { return new SharedPath(); }
        }
    }

    public static final class AudioConfig {
        public static final class Builder {
            public AudioConfig build() { return new AudioConfig(); }
        }
    }

    public static final class UsbConfig {
        public static final class Builder {
            public UsbConfig build() { return new UsbConfig(); }
        }
    }

    public static class DisplayConfig {
        public static class Builder {
            public Builder() {}
            public Builder setWidth(int width) { return this; }
            public Builder setHeight(int height) { return this; }
            public Builder setHorizontalDpi(int horizontalDpi) { return this; }
            public Builder setVerticalDpi(int verticalDpi) { return this; }
            public Builder setRefreshRate(int refreshRate) { return this; }
            public DisplayConfig build() { return new DisplayConfig(); }
        }
    }

    public static class GpuConfig {
        public static class Builder {
            public Builder() {}
            public Builder setBackend(String backend) { return this; }
            public Builder setContextTypes(String[] contextTypes) { return this; }
            public Builder setPciAddress(String pciAddress) { return this; }
            public Builder setRendererFeatures(String rendererFeatures) { return this; }
            public Builder setRendererUseEgl(Boolean useEgl) { return this; }
            public Builder setRendererUseGles(Boolean useGles) { return this; }
            public Builder setRendererUseGlx(Boolean useGlx) { return this; }
            public Builder setRendererUseSurfaceless(Boolean useSurfaceless) { return this; }
            public Builder setRendererUseVulkan(Boolean useVulkan) { return this; }
            public GpuConfig build() { return new GpuConfig(); }
        }
    }

    public static final class Builder {
        public Builder() {}
        public Builder setName(String name) { return this; }
        public Builder setKernelPath(String kernelPath) { return this; }
        public Builder setBootloaderPath(String bootloaderPath) { return this; }
        public Builder setInitrdPath(String initrdPath) { return this; }
        public Builder addDisk(Disk disk) { return this; }
        public Builder addSharedPath(SharedPath path) { return this; }
        public Builder addParam(String param) { return this; }
        public Builder setDisplayConfig(DisplayConfig displayConfig) { return this; }
        public Builder setGpuConfig(GpuConfig gpuConfig) { return this; }
        public Builder useTouch(boolean touch) { return this; }
        public Builder useKeyboard(boolean keyboard) { return this; }
        public Builder useMouse(boolean mouse) { return this; }
        public Builder useSwitches(boolean switches) { return this; }
        public Builder useTrackpad(boolean trackpad) { return this; }
        public Builder useAutoMemoryBalloon(boolean autoMemoryBalloon) { return this; }
        public Builder useNetwork(boolean network) { return this; }
        public Builder setAudioConfig(AudioConfig audioConfig) { return this; }
        public Builder setUsbConfig(UsbConfig usbConfig) { return this; }

        public VirtualMachineCustomImageConfig build() {
            return new VirtualMachineCustomImageConfig();
        }
    }
}
