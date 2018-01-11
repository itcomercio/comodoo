# -*- mode: ruby -*-
# vi: set ft=ruby :

ENV['VAGRANT_DEFAULT_PROVIDER'] = 'libvirt'

Vagrant.configure("2") do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  config.vm.box = "ignisf/debian8-arm"
  # vagrant issues #1673..fixes hang with configure_networks
  config.ssh.shell = "bash -c 'BASH_ENV=/etc/profile exec bash'"
  config.vm.provider :libvirt do |domain|
    domain.uri = 'qemu+unix:///system'
    domain.host = 'virtualized'
    domain.kernel = File.join(Dir.pwd, "vmlinuz")
    domain.initrd = File.join(Dir.pwd, "initrd")
    domain.cmd_line = 'root=/dev/mmcblk0p2 devtmpfs.mount=0 rw'
    domain.memory = 256
    domain.driver = 'qemu'
    domain.machine_type = 'virt'
    domain.machine_arch = 'armv7l'
    domain.cpu_mode = 'custom'
    domain.cpu_model = 'nil'
    domain.cpu_fallback = 'allow'
    domain.graphics_type = 'none'
  end
end
