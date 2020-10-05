```
An approach to key-value storage service #1.

Key ingredients:
	- Binary search tree algorithm for storage engine.
	- CRC-32 checksum algorithm (as hash function).

Known limitations (tested with 100000 random data):
	- big memory footprint due to storage algorithm i use.
	- can only handle 5 forked process.

Available commands:
- GET (key)
- SET (key) (value)
- UPDATE (key) (value)
- DELETE (key) (value)
```
