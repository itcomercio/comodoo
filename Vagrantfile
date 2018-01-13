# -*- mode: ruby -*-
# vi: set ft=ruby :

ENV['VAGRANT_DEFAULT_PROVIDER'] = 'libvirt'

Vagrant.configure("2") do |config|

  config.vm.box = "Fedora-Minimal-armhfp-26-1.5"
  config.ssh.shell = "bash -c 'BASH_ENV=/etc/profile exec bash'"
  config.vm.provider :libvirt do |domain|
    domain.uri = 'qemu+unix:///system'
    domain.host = 'virtualized'
    #domain.kernel = File.join(Dir.pwd, "/home/javiroman/HACK/dev/working/arm/vmlinuz")
    domain.kernel = "/home/javiroman/HACK/dev/working/arm/vmlinuz"
    #domain.initrd = File.join(Dir.pwd, "/home/javiroman/HACK/dev/working/arm/initrd")
    domain.initrd = "/home/javiroman/HACK/dev/working/arm/initrd"
    domain.cmd_line = 'console=ttyAMA0 rw root=LABEL=_/ rootwait'
    domain.memory = 256
    domain.driver = 'qemu'
    domain.machine_type = 'virt'
    # virsh capabilities
    domain.machine_arch = 'armv7l'
    domain.cpu_mode = 'custom'
    domain.cpu_model = 'nil'
    domain.cpu_fallback = 'allow'
    domain.graphics_type = 'none'
  end
end
