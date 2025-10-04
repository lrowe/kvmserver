#include <stddef.h>

/* Resume storage VM with provided data shared two-ways. */
extern void sys_remote_resume(void* data, size_t len);
/* Wait for remote resume (in storage) */
extern size_t sys_storage_wait_paused(void** req);

void remote_resume(void *buffer, size_t len) {
	sys_remote_resume(buffer, len);
}

void* storage_wait_paused()
{
	void *ptr = NULL;
	const size_t bytes = sys_storage_wait_paused(&ptr);
	return ptr;
}

asm(".global sys_remote_resume\n"
	".type sys_remote_resume, @function\n"
	"sys_remote_resume:\n"
	"	mov $0x1070B, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_storage_wait_paused\n"
	".type sys_storage_wait_paused, @function\n"
	"sys_storage_wait_paused:\n"
	".cfi_startproc\n"
	"	mov $0x10002, %eax\n"
	"	out %eax, $0\n"
	"   wrfsbase %rdi\n"
	"	ret\n"
	".cfi_endproc\n");
