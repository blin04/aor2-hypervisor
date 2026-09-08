#include "vm.h"
#include "handler.h"
#include "fileops.h"
#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/ioctl.h>

static pthread_mutex_t io_lock = PTHREAD_MUTEX_INITIALIZER;

static void flush_console_locked(struct vm *v)
{
	if (v->out_len) {
		fwrite(v->out_buf, 1, v->out_len, stdout);
		v->out_len = 0;
	}
}

static void flush_console(struct vm *v)
{
	pthread_mutex_lock(&io_lock);
	flush_console_locked(v);
	fflush(stdout);
	pthread_mutex_unlock(&io_lock);
}

static void vm_log(struct vm *v, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pthread_mutex_lock(&io_lock);
	flush_console_locked(v);
	vprintf(fmt, ap);
	fflush(stdout);
	pthread_mutex_unlock(&io_lock);
	va_end(ap);
}

void* handler(void *arg)
{
	struct vm *v = (struct vm*)arg;

	struct file_operation file_op;
	file_op.code = -1;

	while (v->stop == 0) {
		v->ret = ioctl(v->vcpu_fd, KVM_RUN, 0);
		if (v->ret == -1) {
			vm_log(v, "KVM_RUN failed for guest %s\n", v->image_path);
			vm_destroy(v);
			return NULL;
		}

		switch (v->run->exit_reason) {
		case KVM_EXIT_IO:
			if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == IO_PORT) {
				char *p = (char *)v->run;
				char c = *(p + v->run->io.data_offset);

				v->out_buf[v->out_len++] = c;
				if (c == '\n' || v->out_len == sizeof(v->out_buf))
					flush_console(v);
			}
			else if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == FILE_PORT) {
				char* p = (char *)v->run;
				uint32_t data = (v->run->io.size == 4)
					? *(uint32_t *)(p + v->run->io.data_offset)
					: *(unsigned char *)(p + v->run->io.data_offset);
				file_operation_handler(v, &file_op, data);
			}
			else if (v->run->io.direction == KVM_EXIT_IO_IN && v->run->io.port == FILE_PORT) {
				char *p = (char *)v->run;
				uint32_t* loc = (uint32_t*)(p + v->run->io.data_offset);
				*loc = file_operation_handler(v, &file_op, 0);
			}
			else if (v->run->io.direction == KVM_EXIT_IO_IN && v->run->io.port == IPC_SHARED_PORT) {
				char *p = (char *)v->run;
				uint32_t *loc = (uint32_t *)(p + v->run->io.data_offset);
				if (!v->guest_role_set) {
					// first interrupt handling, give the guest his role
					*loc = (uint32_t)v->guest_role;
					v->guest_role_set = 1;
				} else {
					// todo: implement guest's role
					*loc = 0;
				}
			}
			continue;
		case KVM_EXIT_IRQ_WINDOW_OPEN:
			if (!ipc_is_finished()) {
				if (inject_irq(v, IRQ_NUM) < 0) {
					flush_console(v);
					vm_destroy(v);
					return NULL;
				}
			} else {
				v->run->request_interrupt_window = 0;
			}
			continue;
		case KVM_EXIT_HLT:
			vm_log(v, "VM %d (image: %s) finished!\n", v->id, v->image_path);
			v->stop = 1;
			break;
		case KVM_EXIT_SHUTDOWN:
			vm_log(v, "Shutdown\n");
			v->stop = 1;
			break;
		case KVM_EXIT_FAIL_ENTRY:
			vm_log(v, "VM stopped: guest %s failed to enter guest mode, "
			       "hardware entry failure reason 0x%llx\n",
			       v->image_path,
			       (unsigned long long)v->run->fail_entry.hardware_entry_failure_reason);
			v->stop = 1;
			break;
		case KVM_EXIT_INTERNAL_ERROR:
			vm_log(v, "VM stopped: guest %s internal KVM error, suberror %u\n",
			       v->image_path, v->run->internal.suberror);
			v->stop = 1;
			break;
		default:
			vm_log(v, "Default - guest %s exit reason: %d\n",
			       v->image_path, v->run->exit_reason);
			break;
		}
	}
	return NULL;
}