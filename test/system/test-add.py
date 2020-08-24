from helper import SystemTest
import unittest

class AddTest(SystemTest):

  def test_add(self):
    all_data = ((b'\xaa' * 256) + (b'\xbb' * 256) + (b'\xcc' * 256)) * 3

    # Create a new dataset.
    dataset = self.reserveDataset()
    result = self.cmd('btoep-add', '--dataset', dataset, '--offset=512', input=all_data[512:640])
    self.assertEqual(result.stdout, b'')
    self.assertEqual(result.stderr, b'')
    self.assertEqual(self.readDataset(dataset)[512:640], all_data[512:640])
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\x7f')

    # Add another, separate range.
    result = self.cmd('btoep-add', '--dataset', dataset, '--offset=1024', input=all_data[1024:1152])
    self.assertEqual(result.stdout, b'')
    self.assertEqual(result.stderr, b'')
    self.assertEqual(self.readDataset(dataset)[512:640], all_data[512:640])
    self.assertEqual(self.readDataset(dataset)[1024:1152], all_data[1024:1152])
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\x7f\xff\x02\x7f')

    # Add a range that merges both existing ranges, but does not overlap.
    result = self.cmd('btoep-add', '--dataset', dataset, '--offset=640', input=all_data[640:1024])
    self.assertEqual(result.stdout, b'')
    self.assertEqual(result.stderr, b'')
    self.assertEqual(self.readDataset(dataset)[512:1152], all_data[512:1152])
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\xff\x04')

    # Now add a superset.
    result = self.cmd('btoep-add', '--dataset', dataset, '--offset=256', input=all_data[256:1280])
    self.assertEqual(result.stdout, b'')
    self.assertEqual(result.stderr, b'')
    self.assertEqual(self.readDataset(dataset)[256:1280], all_data[256:1280])
    self.assertEqual(self.readIndex(dataset), b'\x80\x02\xff\x07')

    # TODO: Test conflicts, overwrite, etc.

if __name__ == '__main__':
  unittest.main()
