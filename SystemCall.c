#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/mm.h>
 
 
MODULE_LICENSE("Dual BSD/GPL");

#define SYS_CALL_TABLE_ADDRESS 0xffffffff81a00200
#define VICTIM_CALL_NUM 223         
#define ADDR_CALL_NUM 224

int orig_cr0;
unsigned long *sys_call_table = 0;

unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
uint8_t unused2[64];
uint8_t temp = 0;

 
static void(*syscall_saved_victim)(size_t, size_t);
static uint8_t*(*syscall_saved_addr)(uint8_t*);
 
static int clear_cr0(void) {
    unsigned int cr0=0;
    unsigned int ret;
    asm volatile("movq %%cr0,%%rax":"=a"(cr0));
    ret=cr0;
    cr0&=0xfffffffffffeffff;
    asm volatile("movq %%rax,%%cr0"::"a"(cr0));
    return ret;
}
 
static void setback_cr0(int val) {
    asm volatile("movq %%rax,%%cr0"::"a"(val));
}
 
static void sys_mycall_victim(size_t offset, uint8_t* shared_arr2) {
	volatile int z;

    for (z = 0; z < 100; z++) {} /* Delay (can also mfence) */
    asm volatile ("clflush 0(%0)\n" : : "c"(&array1_size) : "eax");
    for (z = 0; z < 100; z++) {} /* Delay (can also mfence) */
    asm volatile ("clflush 0(%0)\n" : : "c"(&offset) : "eax");
    for (z = 0; z < 100; z++) {} /* Delay (can also mfence) */

    if (offset < array1_size) {
		temp &= shared_arr2[array1[offset] * 4096];
	} 
}

static uint8_t* sys_mycall_addr(uint8_t* phy_addr) {
	uint8_t* virt_addr = (uint8_t*)phys_to_virt((long long unsigned int)phy_addr);
	if (phy_addr == (uint8_t*)0) {
		return array1;
	}
	else {
	    printk("GYX: Phy addr is: 0x%p\n", phy_addr);
	    printk("GYX: Vir addr is: 0x%p\n", virt_addr);	
	}
    return virt_addr;
}

static int __init call_init(void) {
	

    sys_call_table = (unsigned long*)(SYS_CALL_TABLE_ADDRESS);
    printk("GYX: call_init......\n");
    
	// Victim
    syscall_saved_victim=(void(*)(size_t, size_t))(sys_call_table[VICTIM_CALL_NUM]);
    orig_cr0=clear_cr0();                                                         
    sys_call_table[VICTIM_CALL_NUM]=(unsigned long) &sys_mycall_victim;
    setback_cr0(orig_cr0);

    // Addr
    syscall_saved_addr=(uint8_t*(*)(uint8_t*))(sys_call_table[ADDR_CALL_NUM]);              
    orig_cr0=clear_cr0();                                               
    sys_call_table[ADDR_CALL_NUM]=(unsigned long) &sys_mycall_addr; 
    setback_cr0(orig_cr0);                                              


    return 0;
}
 
static void __exit call_exit(void) {
    printk("GYX: Call_exit......\n");
    orig_cr0=clear_cr0();
    sys_call_table[VICTIM_CALL_NUM]=(unsigned long)syscall_saved_victim;
    sys_call_table[ADDR_CALL_NUM] = (unsigned long)syscall_saved_addr;
    setback_cr0(orig_cr0);
}
 
module_init(call_init);
module_exit(call_exit);
