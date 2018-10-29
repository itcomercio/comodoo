import dialog

try:
    dialog.Dialog().msgbox("Hi")
except Exception as e:
    print e.message
