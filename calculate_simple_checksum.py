#!/usr/bin/python

f = open("firmware2.bin", "rb")
characters = f.read()
f.close()

checksum = 0
bytes_seen = 0
bytes_checksummed = 0
for b in characters:
  bytes_seen += 1
  if bytes_seen <= 128 * 256:
    checksum = checksum + ord(b)
    bytes_checksummed += 1
print "Checksummed %d bytes, got %d." % (bytes_checksummed, checksum % 65536)
