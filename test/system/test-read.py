from helper import ExitCode, SystemTest
import unittest

class ReadTest(SystemTest):

  def test_info(self):
    self.assertInfo('btoep-read', [
      '--dataset', '--index-path', '--lockfile-path',
      '--offset', '--length', '--limit'
    ])

  def getCmdArgs(self, dataset, offset=None, length=None, limit=None):
    args = ['btoep-read', '--dataset', dataset]
    if offset is not None:
      args.append('--offset=' + str(offset))
    if length is not None:
      args.append('--length=' + str(length))
    if limit is not None:
      args.append('--limit=' + str(limit))
    return args

  def cmdRead(self, dataset, **kwargs):
    return self.cmd_stdout(self.getCmdArgs(dataset, **kwargs))

  def assertOutOfBounds(self, dataset, **kwargs):
    self.cmd(self.getCmdArgs(dataset, **kwargs),
             expected_returncode = ExitCode.APP_ERROR,
             expected_stderr = 'Error: Read out of bounds\n')

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

    # Test the --limit option with the previous dataset
    self.assertEqual(self.cmdRead(dataset, limit=10), b'')
    self.assertOutOfBounds(dataset, length=2, limit=1)
    self.assertOutOfBounds(dataset, length=2, limit=10)
    self.assertEqual(self.cmdRead(dataset, offset=128), b'')
    self.assertEqual(self.cmdRead(dataset, offset=129), b'\x0a' * 128)
    self.assertEqual(self.cmdRead(dataset, offset=129, length=64), b'\x0a' * 64)
    self.assertEqual(self.cmdRead(dataset, offset=129, length=64, limit=100), b'\x0a' * 64)
    self.assertEqual(self.cmdRead(dataset, offset=256, length=10, limit=1), b'\x0a')
    self.assertEqual(self.cmdRead(dataset, offset=256, limit=9), b'\x0a')
    self.assertEqual(self.cmdRead(dataset, offset=258, limit=10), b'\x0a' * 10)
    self.assertEqual(self.cmdRead(dataset, offset=259, limit=128), b'\x0a' * 127)

    # Test a range that doesn't fit into the internal 64 KiB buffer
    dataset = self.createDataset(b'\x0a' * 1024 * 512, b'\x00\xff\xff\x1f')
    self.assertEqual(self.cmdRead(dataset), b'\x0a' * 1024 * 512)
    self.assertEqual(self.cmdRead(dataset, offset=1024 * 511), b'\x0a' * 1024)
    self.assertOutOfBounds(dataset, length=1024 * 513)

if __name__ == '__main__':
  unittest.main()
