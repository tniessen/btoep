from helper import SystemTest
import unittest

class ListRangesTest(SystemTest):

  def cmdListRanges(self, dataset, missing = False):
    args = ['btoep-list-ranges', '--dataset', dataset]
    if missing:
      args.append('--missing')
    result = self.cmd(*args)
    self.assertEqual(result.stderr, b'')
    return result.stdout

  def test_list_ranges(self):
    # Test an empty dataset with an empty index
    dataset = self.createDataset(b'', b'')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, b'')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, b'')

    # Test a 512 KiB dataset with an empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, b'')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, b'0..524287\n')

    # Test a 512 KiB dataset with a non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x00\x00')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, b'0..0\n')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, b'1..524287\n')

    # Test a 512 KiB dataset with another non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x81\x01\x7f\x00\x7f')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, b'129..256\n258..385\n')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, b'0..128\n257..257\n386..524287\n')

if __name__ == '__main__':
  unittest.main()
