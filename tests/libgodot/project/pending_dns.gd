extends Node


func _ready() -> void:
	# Leave work queued when the host immediately stops/destroys the instance.
	for index in range(256):
		IP.resolve_hostname_queue_item("localhost", IP.TYPE_ANY)
