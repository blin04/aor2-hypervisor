#include "vm.h"

#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <getopt.h>

int main(int argc, char *argv[])
{
	// cli options
	struct option options[] = {
		{"memory", required_argument, NULL, 'm'},
		{"page", required_argument, NULL, 'p'},
		{"guest", required_argument, NULL, 'g'},
		{0, 0, 0, 0}
	};

	int ret;
	int n_guests = 0;
	char** guests = (char** )malloc(sizeof(char *));

	if (guests == NULL) {
		printf("error: malloc failed\n");
		return 1;
	}

	// parse arguments
	while ((ret = getopt_long(argc, argv, "m:p:g:", options, NULL)) != -1) {
		switch (ret) {
		case 'm':
			printf("parsed m with arg %s\n", optarg);
			break;
		case 'p':
			printf("parsed p with arg %s\n", optarg);
			break;
		case 'g':
			printf("parsed g with args: ");
		
			// first guest image is parsed by getopt
			guests[0] = optarg;
			n_guests++;

			// parse the rest of guest images
			while (optind < argc && argv[optind][0] != '-') {
				guests = realloc(guests, (n_guests + 1) * sizeof(char *));
				guests[n_guests++] = argv[optind];
				optind++;
			}

			for (int i = 0; i < n_guests; i++) {
				if (i != 0) printf(", ");
				printf("%s", guests[i]);
			}				
			printf("\n");

			break;
		case '?':
			if (optopt == 'm') {
				printf("error: missing guest memory size (-m, --memory)\n");
				return 0;
			}
			else if (optopt == 'p') {
				printf("error: missing page size (-p, --page)\n");
				return 0;
			}
			else if (optopt == 'g') {
				printf("error: missing page size (-g, --guest)\n");
				return 0;
			}
		}
	}

	/* struct vm v;
	struct kvm_sregs sregs;
	struct kvm_regs regs;
	int stop = 0;
	int ret = 0;
	int irq_pending = IRQ_COUNT;

	if (argc != 2) {
		printf("The program requests an image to run: %s <guest-image>\n", argv[0]);
		return 1;
	}

	/* VM initialization */
	/*

	if (vm_init(&v, MEM_SIZE)) {
		printf("Failed to init the VM\n");
		return 1;
	}

	if (ioctl(v.vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		vm_destroy(&v);
		return 1;
	}

	setup_long_mode(&v, &sregs);

	if (ioctl(v.vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		vm_destroy(&v);
		return 1;
	}

	if (load_guest_image(&v, argv[1], GUEST_START_ADDR) < 0) {
		printf("Failed to load guest image\n");
		vm_destroy(&v);
		return 1;
	}

	memset(&regs, 0, sizeof(regs));
	regs.rflags = 0x2;
	regs.rip    = 0;
	regs.rsp    = 2 << 20;

	if (ioctl(v.vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		vm_destroy(&v);
		return 1;
	}

	v.run->request_interrupt_window = (irq_pending > 0);

	while (stop == 0) {
		ret = ioctl(v.vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			printf("KVM_RUN failed\n");
			vm_destroy(&v);
			return 1;
		}

		switch (v.run->exit_reason) {
		case KVM_EXIT_IO:
			if (v.run->io.direction == KVM_EXIT_IO_OUT && v.run->io.port == 0xE9) {
				char *p = (char *)v.run;
				printf("%c", *(p + v.run->io.data_offset));
			}
			continue;
		case KVM_EXIT_IRQ_WINDOW_OPEN:
			if (irq_pending > 0) {
				if (inject_irq(&v, IRQ_NUM) < 0) {
					vm_destroy(&v);
					return 1;
				}
				irq_pending--;
			} else {
				v.run->request_interrupt_window = 0;
			}
			continue;
		case KVM_EXIT_HLT:
			printf("KVM_EXIT_HLT\n");
			stop = 1;
			break;
		case KVM_EXIT_SHUTDOWN:
			printf("Shutdown\n");
			stop = 1;
			break;
		default:
			printf("Default - exit reason: %d\n", v.run->exit_reason);
			break;
		}
	}

	vm_destroy(&v); */
	return 0;
}
