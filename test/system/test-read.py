from helper import SystemTest
import unittest

class FindOffsetTest(SystemTest):

  def cmdRead(self, dataset, offset=None, length=None):
    args = ['btoep-read', '--dataset', dataset]
    if offset is not None:
      args.append('--offset=' + str(offset))
    if length is not None:
      args.append('--length=' + str(length))
    result = self.cmd(*args)
    self.assertEqual(result.stderr, b'')
    return result.stdout

  def assertOutOfBounds(self, dataset, offset=None, length=None):
    args = ['btoep-read', '--dataset', dataset]
    if offset is not None:
      args.append('--offset=' + str(offset))
    if length is not None:
      args.append('--length=' + str(length))
    result = self.cmd(*args, check=False, text=True)
    self.assertEqual(result.stderr, 'Error: Read out of bounds\n')

  def test_read(self):
    # Test an empty dataset with an empty index
    dataset = self.createDataset(b'', b'')
    self.assertEqual(self.cmdRead(dataset), b'')
    self.assertEqual(self.cmdRead(dataset, offset=20), b'')
    self.assertEqual(self.cmdRead(dataset, offset=20, length=0), b'')
    self.assertEqual(self.cmdRead(dataset, offset=100), b'')
    self.assertOutOfBounds(dataset, length=1)
    self.assertOutOfBounds(dataset, offset=20, length=1)
    self.assertOutOfBounds(dataset, length=100)

    # Test a 512 KiB dataset with a non-empty index
    dataset = self.createDataset(b'\x99' * 1024 * 512, b'\x00\x00')
    self.assertEqual(self.cmdRead(dataset), b'\x99')
    self.assertEqual(self.cmdRead(dataset, length=1), b'\x99')
    self.assertEqual(self.cmdRead(dataset, offset=2), b'')
    self.assertOutOfBounds(dataset, offset=1, length=1)
    self.assertOutOfBounds(dataset, length=2)

    # Test a 512 KiB dataset with another non-empty index
    dataset = self.createDataset(b'\x0a' * 1024 * 512, b'\x81\x01\x7f\x00\x7f')
    self.assertEqual(self.cmdRead(dataset), b'')
    self.assertEqual(self.cmdRead(dataset, offset=128), b'')
    self.assertEqual(self.cmdRead(dataset, offset=129), b'\x0a' * 128)
    self.assertEqual(self.cmdRead(dataset, offset=129, length=64), b'\x0a' * 64)
    self.assertEqual(self.cmdRead(dataset, offset=256, length=1), b'\x0a')
    self.assertOutOfBounds(dataset, offset=129, length=129)
    self.assertOutOfBounds(dataset, offset=256, length=2)
    self.assertOutOfBounds(dataset, offset=257, length=1)
    self.assertEqual(self.cmdRead(dataset, offset=257), b'')
    self.assertEqual(self.cmdRead(dataset, offset=258), b'\x0a' * 128)
    self.assertEqual(self.cmdRead(dataset, offset=259), b'\x0a' * 127)
    self.assertOutOfBounds(dataset, offset=258, length=1024)
    self.assertEqual(self.cmdRead(dataset, offset=385), b'\x0a')
    self.assertEqual(self.cmdRead(dataset, offset=386), b'')

    # Test a range that doesn't fit into the internal 64 KiB buffer
    dataset = self.createDataset(b'\x0a' * 1024 * 512, b'\x00\xff\xff\x1f')
    self.assertEqual(self.cmdRead(dataset), b'\x0a' * 1024 * 512)
    self.assertEqual(self.cmdRead(dataset, offset=1024 * 511), b'\x0a' * 1024)
    self.assertOutOfBounds(dataset, length=1024 * 513)

if __name__ == '__main__':
  unittest.main()
