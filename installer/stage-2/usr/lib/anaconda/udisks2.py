import dbus

#for service in dbus.SystemBus().list_names():
#    print(service)

def get_device(device):
    bus = dbus.SystemBus()
    bd = bus.get_object('org.freedesktop.UDisks2', '/org/freedesktop/UDisks2/block_devices%s'%device[4:])
    try:
        device = bd.Get('org.freedesktop.UDisks2.Block', 'Device', dbus_interface='org.freedesktop.DBus.Properties')
        device = bytearray(device).replace(b'\x00', b'').decode('utf-8')
        print "printing " + device
        label = bd.Get('org.freedesktop.UDisks2.Block', 'IdLabel', dbus_interface='org.freedesktop.DBus.Properties')
        print 'Name od partition is %s'%label
        uuid = bd.Get('org.freedesktop.UDisks2.Block', 'IdUUID', dbus_interface='org.freedesktop.DBus.Properties')
        print 'UUID is %s'%uuid
        size = bd.Get('org.freedesktop.UDisks2.Block', 'Size', dbus_interface='org.freedesktop.DBus.Properties')
        print 'Size is %s'%uuid
        file_system =  bd.Get('org.freedesktop.UDisks2.Block', 'IdType', dbus_interface='org.freedesktop.DBus.Properties')
        print 'Filesystem is %s'%file_system
    except:
        print "Error detecting USB details..."

def get_devices_by_type(type=None):
    devices = []

    bus = dbus.SystemBus()

    # To interact with a remote object, you use a proxy object
    # We get a proxy for the well-known name org.freedesktop.UDisk2 which
    # exports an object whose object path is /org/freedesktop/UDisk2.
    proxy = bus.get_object('org.freedesktop.UDisks2',
                           '/org/freedesktop/UDisks2')

    # D-Bus uses interfaces to provide a namespacing mechanism for methods.
    # An interface is a group of related methods and signals, identified by
    # a name which is a series of dot-separated components starting with
    # a reversed domain name.
    iface = dbus.Interface(proxy, 'org.freedesktop.DBus.ObjectManager')
    try:
        # returns all objects, interfaces and properties in a single
        # method call. In k the path, in v the methods.
        for k,v in iface.GetManagedObjects().iteritems():
            drive_info = v.get('org.freedesktop.UDisks2.Block', {})
            #if drive_info.get('IdUsage') == "filesystem" and not
            # drive_info.get('HintSystem') and not drive_info.get('ReadOnly'):
            device = drive_info.get('Device')
            if device:
                device = bytearray(device).replace(b'\x00', b'').decode('utf-8')
                devices.append(device)
    except Exception as e:
        print(e)
        print "No device found..."

    return devices

