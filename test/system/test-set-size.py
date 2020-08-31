from helper import ExitCode, SystemTest
import os
import unittest

class SetSizeTest(SystemTest):

  def test_info(self):
    self.assertInfo('btoep-set-size', [
      '--dataset', '--index-path', '--lockfile-path',
      '--size', '--force'
    ])

  def assertSetSize(self, dataset, size, force = False):
    args = ['btoep-set-size', '--dataset', dataset, '--size', str(size)]
    if force:
      args.append('--force')
    self.cmd(args)
    self.assertEqual(size, os.path.getsize(dataset))

  def assertFailDestructive(self, dataset, size):
    self.cmd(['btoep-set-size', '--dataset', dataset, '--size', str(size)],
             expected_returncode = ExitCode.APP_ERROR,
             expected_stderr = 'Error: Destructive action\n')

  def test_set_size(self):
    all_data = ((b'\xaa' * 256) + (b'\xbb' * 256) + (b'\xcc' * 256)) * 3

    # Create a new dataset.
    dataset = self.reserveDataset()
    self.assertSetSize(dataset, 10001)
    self.assertEqual(self.readDataset(dataset)[0:10001], b'\x00' * 10001)
    self.assertEqual(self.readIndex(dataset), b'')

    # Create a 512 byte file without missing ranges, and extend it to 1 KiB.
    dataset = self.createDataset(b'\x11' * 256, b'\x00\xff\x03')
    self.assertSetSize(dataset, 1024)
    # Now shrink it back to 512 bytes.
    self.assertSetSize(dataset, 512)
    self.assertEqual(self.readDataset(dataset)[0:256], b'\x11' * 256)
    # Shrinking further should not be possible, unless specifying --force.
    for size in [511, 256, 1, 0]:
      self.assertFailDestructive(dataset, size)
      self.assertSetSize(dataset, size, force=True)
    # The index should now be empty.
    self.assertEqual(self.readIndex(dataset), b'')

if __name__ == '__main__':
  unittest.main()
