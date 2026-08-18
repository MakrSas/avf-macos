package android.system.virtualmachine;

public class VirtualMachineException extends Exception {
    public VirtualMachineException() {}
    public VirtualMachineException(String message) { super(message); }
    public VirtualMachineException(String message, Throwable cause) { super(message, cause); }
}
