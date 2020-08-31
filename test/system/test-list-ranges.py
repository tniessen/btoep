from helper import SystemTest
import unittest

class ListRangesTest(SystemTest):

  def test_info(self):
    self.assertInfo('btoep-list-ranges', [
      '--dataset', '--index-path', '--lockfile-path',
      '--range-format', '--missing'
    ])

  def cmdListRanges(self, dataset, missing = False, format = None):
    args = ['btoep-list-ranges', '--dataset', dataset]
    if missing:
      args.append('--missing')
    if format is not None:
      args.append('--range-format=' + format)
    return self.cmd_stdout(args, text=True)

  def test_list_ranges(self):
    # Test an empty dataset with an empty index
    dataset = self.createDataset(b'', b'')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, '')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, '')

    # Test a 512 KiB dataset with an empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, '')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, '0..524287\n')

    # Test a 512 KiB dataset with a non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x00\x00')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, '0..0\n')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, '1..524287\n')

    # Test the same dataset again, but using a different format
    ranges = self.cmdListRanges(dataset, format='exclusive')
    self.assertEqual(ranges, '0...1\n')
    ranges = self.cmdListRanges(dataset, missing=True, format='exclusive')
    self.assertEqual(ranges, '1...524288\n')

    # Test a 512 KiB dataset with another non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x81\x01\x7f\x00\x7f')
    ranges = self.cmdListRanges(dataset)
    self.assertEqual(ranges, '129..256\n258..385\n')
    ranges = self.cmdListRanges(dataset, True)
    self.assertEqual(ranges, '0..128\n257..257\n386..524287\n')

if __name__ == '__main__':
  unittest.main()
