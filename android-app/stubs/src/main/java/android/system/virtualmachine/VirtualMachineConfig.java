package android.system.virtualmachine;

import android.content.Context;

public class VirtualMachineConfig {
    public static final class Builder {
        public Builder(Context context) {}

        public Builder setCustomImageConfig(VirtualMachineCustomImageConfig customImageConfig) {
            return this;
        }

        public Builder setMemoryBytes(long memoryBytes) {
            return this;
        }

        public Builder setProtectedVm(boolean protectedVm) {
            return this;
        }

        public Builder setPayloadBinaryName(String name) {
            return this;
        }

        public Builder setPayloadConfigPath(String path) {
            return this;
        }

        public VirtualMachineConfig build() {
            return new VirtualMachineConfig();
        }
    }
}
