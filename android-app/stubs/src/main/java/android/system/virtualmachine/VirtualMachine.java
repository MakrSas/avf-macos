package android.system.virtualmachine;

import java.util.concurrent.Executor;

public class VirtualMachine {
    public void run() throws VirtualMachineException {}
    public void stop() throws VirtualMachineException {}
    public void setCallback(Executor executor, VirtualMachineCallback callback) {}
}
