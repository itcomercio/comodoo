# -*- mode: ruby -*-
# vim: set ft=ruby ts=2 et :

VAGRANTFILE_API_VERSION = "2"

# Tested with Vagrant version:
Vagrant.require_version ">= 1.7.2"

BOX       =  "fedora/34-cloud-base"
CPU       = 4
MEM       = 8192
VM_NAME   = "builder"
HOST_NAME = "builder"

$provision = <<-SCRIPT
echo "[PROVISIONER] Installing dependencies ..."
sudo dnf install -y -q git \
echo "[PROVISIONER] Disabling SELINUX ..."
sudo sed -i s/^SELINUX=.*$/SELINUX=permissive/ /etc/selinux/config
sudo setenforce 0
echo "[PROVISIONER] Done." 
SCRIPT

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.synced_folder ".", "/vagrant", disabled: true

  config.vm.define VM_NAME do |node|
    node.vm.box = BOX
    node.vm.hostname = HOST_NAME
    node.vm.provider :libvirt do |domain|
      domain.uri = 'qemu+unix:///system'
      domain.driver = 'kvm'
      domain.memory = MEM
      domain.cpus = CPU
      # https://bugzilla.redhat.com/show_bug.cgi?id=1706289
      domain.qemu_use_session = false
    end # provider
  end # define

	config.vm.provision "shell", inline: $provision
end # vagrant
