KVM Udev Util
=============

This small utility program can be run while tracing virtual machines. It will listen on a udev "misc" subsystem for any event on device "dev/kvm" and add a trace event when a KVM virtual machine is created or destroyed.

** Disclaimer ** This is a prototype to show that it works. It may eventually reside in some lttng-related project.

Building
--------

Simply type

    make

in the directory.

Using
-----

This program can be used along with LTTng to trace virtual machine creation and destruction.

In one window, one can simple run this program and click Ctrl-C when done tracing

    ./kvm_udev

In another window, one can trace using lttng

    lttng create
    lttng enable-event -u kvm_udev:*
    lttng start
    <Create and destroy VMs>
    lttng stop