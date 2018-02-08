/*
 * Copyright (C) 2018 Geneviève Bastien <gbastien@versatic.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*
 * This library listens to the udev interface and traces kvm virtual machine
 * creations and destructions.
 */
 
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>

/*
 * We need to define TRACEPOINT_DEFINE in one C file in the program
 * before including provider headers.
 */
#define TRACEPOINT_DEFINE
#include "kvm_udev_provider.h"

#define KVM_NODE "/dev/kvm"
#define KVM_EVENT_CREATE "create"
#define KVM_EVENT_DESTROY "destroy"
#define KVM_UUID_PARAM "-uuid"
#define QEMU_KVM_UUID_LEN	37

int doTraceKvmCreate(int pid) {
	FILE *file;
	char *line = NULL, uuid[QEMU_KVM_UUID_LEN];
	size_t len = 0;
	char cmdline_file[64];
	uuid[0] = '\0';

	if (pid > 0) {
		sprintf(cmdline_file, "/proc/%d/cmdline", pid);
		file = fopen(cmdline_file, "r");
		if (file) {
		    while (getdelim(&line, &len, '\0', file) != -1) {
		    	if (!strcmp(KVM_UUID_PARAM, line)) {
		    		// Read the next value in uuid
		    		if (getdelim(&line, &len, '\0', file) != -1)
		    			strcpy(uuid, line);
		    	}
		    }
		    free(line);
		    fclose(file);
		}
	}
	tracepoint(kvm_udev, kvm_created, pid, uuid);

	return 0;
}

int doTraceKvmEvent(struct udev_device * dev) {
	struct udev_list_entry *list, *current;
	const char *event, *pidstr;
	int pid = -1;

	if (!tracepoint_enabled(kvm_udev, kvm_created)) {
		// No need to do anything
	    return 0;
	}

	list = udev_device_get_properties_list_entry(dev);
	current = udev_list_entry_get_by_name(list, "EVENT");
	if (current) {
		event = udev_list_entry_get_value(current);
	}
	current = udev_list_entry_get_by_name(list, "PID");
	if (current) {
		pidstr = udev_list_entry_get_value(current);
		pid = strtoul(pidstr, NULL, 0);
	}
	if (!strcmp(KVM_EVENT_CREATE, event)) {
		doTraceKvmCreate(pid);
	} else if (!strcmp(KVM_EVENT_DESTROY, event)) {
		tracepoint(kvm_udev, kvm_destroyed, pid);
	}

	return 0;
}

int main (void)
{
	struct udev *udev;
	struct udev_device *dev;
	struct udev_monitor *mon;
	int fd;
	const char *node;

	/* Create the udev object */
	udev = udev_new();
	if (!udev) {
		printf("Can't create udev\n");
		exit(1);
	}

	/* Set up a monitor to monitor the misc devices */
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "misc", NULL);
	udev_monitor_enable_receiving(mon);
	/* Get the file descriptor (fd) for the monitor.
	   This fd will get passed to select() */
	fd = udev_monitor_get_fd(mon);

	/* This section will run continuously, calling usleep() at
	   the end of each pass. This is to demonstrate how to use
	   a udev_monitor in a non-blocking way. */
	while (1) {
		/* Set up the call to select(). In this case, select() will
		   only operate on a single file descriptor, the one
		   associated with our udev_monitor. Note that the timeval
		   object is set to 0, which will cause select() to not
		   block. */
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		ret = select(fd+1, &fds, NULL, NULL, &tv);

		/* Check if our file descriptor has received data. */
		if (ret > 0 && FD_ISSET(fd, &fds)) {

			/* Make the call to receive the device.
			   select() ensured that this will not block. */
			dev = udev_monitor_receive_device(mon);
			if (dev) {
				node = udev_device_get_devnode(dev);
				if (!strcmp(KVM_NODE, node)) {
					doTraceKvmEvent(dev);
				}

				udev_device_unref(dev);
			}
		}
		usleep(250*1000);
	}

	return 0;
}
