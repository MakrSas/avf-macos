package android.system.virtualmachine;

public class VirtualMachineManager {
    public static final int CAPABILITY_PROTECTED_VM = 1;
    public static final int CAPABILITY_NON_PROTECTED_VM = 2;

    public int getCapabilities() { return 0; }

    public VirtualMachine getOrCreate(String name, VirtualMachineConfig config)
            throws VirtualMachineException {
        return null;
    }
}
