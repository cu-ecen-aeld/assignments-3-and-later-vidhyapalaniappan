FAULTY OOPS KERNEL ERROR

# echo “hello_world” > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=00000000420a5000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#2] SMP
Modules linked in: hello(O) scull(O) faulty(O)
CPU: 0 PID: 164 Comm: sh Tainted: G      D    O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008cc3d80
x29: ffffffc008cc3d80 x28: ffffff80020d0cc0 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 000000556dd22670
x20: 000000556dd22670 x19: ffffff8002080200 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f0000 x3 : ffffffc008cc3df0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 1fc73992a2f4decb ]---


ANALYSIS:

The common result of deferencing a NULL pointer is an oops message. All of the addresses that the CPU uses are virtual addresses that are translated to physical locations via a complicated page table structure. When we dereference an incorrect pointer (NULL in this case), the paging mechanism is unable to perform this mapping. The faulty_write function, which is located in miscellaneous modules/faulty.c, is where the error was made.

Every address in Linux is a virtual address that is translated to a physical address using page tables. When an erroneous NULL pointer dereference takes place, the paging system is unable to map the pointer to a physical address, resulting in a page fault.

1. echo "hello_world" > /dev/faulty: The "hello_world" string will be attempted to be written to the /dev/faulty device file using this command.

2. Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000: The problem was brought about by the kernel's attempt to retrieve a NULL pointer. Dereferencing a NULL pointer can result in a system crash, hence this is a significant error.

3. Mem abort info : reveals details regarding the memory access violation. It shows that the Data Abort (DABT) with read access (WnR = 1) was the exception.

4. Internal error: Oops: 96000045 [#2] SMP: The error code for this line is 96000045, and it specifies the type of mistake as a Oops. Symmetric Multi-Processing (SMP), which denotes a multi-core system, is what the "SMP" stands for.

5. CPU: 0 PID: 164 Comm: sh Tainted: G D O 5.15.18 #1: Information about the CPU and process implicated in the accident is provided on this line. Due to the presence of specific kernel modules, the "Tainted" column suggests that the kernel may be unstable. 

6. pc: faulty_write+0x14/0x20 [faulty]: The program counter (pc) shows that the "faulty" kernel module's faulty_write function is where the crash happened.

7. lr: vfs_write+0xa8/0x2b0: The virtual file system layer's vfs_write function is referenced by the link register (lr).

8. Call trace: An analysis of the functions called prior to the accident is provided in this section. It displays the order of function calls prior to the crash. The faulty_write function was where the crash most likely started.

ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}

The oops message shows the processor's state and CPU register contents at the time of the fault. The process is then terminated when the message has been displayed.

The call trace dump is the additional message that appears after the oops message. In our example, the function call is not particularly complex; the faulty_write method merely invokes the ksys_write function and displays the appropriate position along with a code memory dump up to the fault site.
