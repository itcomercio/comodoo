import dbus

#for service in dbus.SystemBus().list_names():
#    print(service)

def get_usb():
    devices = []
    bus = dbus.SystemBus()
    ud_manager_obj = bus.get_object('org.freedesktop.UDisks2', '/org/freedesktop/UDisks2')
    om = dbus.Interface(ud_manager_obj, 'org.freedesktop.DBus.ObjectManager')
    try:
        for k,v in om.GetManagedObjects().iteritems():
            drive_info = v.get('org.freedesktop.UDisks2.Block', {})
            print drive_info
            if drive_info.get('IdUsage') == "filesystem" and not drive_info.get('HintSystem') and not drive_info.get('ReadOnly'):
                device = drive_info.get('Device')
                device = bytearray(device).replace(b'\x00', b'').decode('utf-8')
                devices.append(device)
    except:
        print "No device found..."
    return devices


def usb_details(device):
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

get_usb()
