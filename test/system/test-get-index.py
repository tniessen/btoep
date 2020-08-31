from helper import SystemTest
import unittest

class GetIndexTest(SystemTest):

  def test_info(self):
    self.assertInfo('btoep-get-index', [
      '--dataset', '--index-path', '--lockfile-path',
      '--min-range-length'
    ])

  def cmdGetIndex(self, dataset, min_range_length=None):
    args = ['btoep-get-index', '--dataset', dataset]
    if min_range_length is not None:
      args.append('--min-range-length=' + str(min_range_length))
    result = self.cmd(*args)
    self.assertEqual(result.stderr, b'')
    return result.stdout

  def test_get_index(self):
    # Test an empty dataset with an empty index
    dataset = self.createDataset(b'', b'')
    index = self.cmdGetIndex(dataset)
    self.assertEqual(index, b'')
    index = self.cmdGetIndex(dataset, min_range_length=32)
    self.assertEqual(index, b'')

    # Test a 512 KiB dataset with a non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x00\x00')
    index = self.cmdGetIndex(dataset)
    self.assertEqual(index, b'\x00\x00')
    index = self.cmdGetIndex(dataset, min_range_length=1)
    self.assertEqual(index, b'\x00\x00')
    index = self.cmdGetIndex(dataset, min_range_length=2)
    self.assertEqual(index, b'')

    # Test a 512 KiB dataset with another non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x81\x01\x7f\x00\x7f')
    index = self.cmdGetIndex(dataset)
    self.assertEqual(index, b'\x81\x01\x7f\x00\x7f')
    index = self.cmdGetIndex(dataset, min_range_length=128)
    self.assertEqual(index, b'\x81\x01\x7f\x00\x7f')
    index = self.cmdGetIndex(dataset, min_range_length=129)
    self.assertEqual(index, b'')

    # Test a 512 KiB dataset with a highly fragmented index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x00\x00' * 20000)
    index = self.cmdGetIndex(dataset)
    self.assertEqual(index, b'\x00\x00' * 20000)
    index = self.cmdGetIndex(dataset, min_range_length=1)
    self.assertEqual(index, b'\x00\x00' * 20000)
    index = self.cmdGetIndex(dataset, min_range_length=2)
    self.assertEqual(index, b'')

    # Another highly fragmented index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x00\xff\x01\x7f\x7f' * 10000)
    index = self.cmdGetIndex(dataset)
    self.assertEqual(index, b'\x00\xff\x01\x7f\x7f' * 10000)
    index = self.cmdGetIndex(dataset, min_range_length=128)
    self.assertEqual(index, b'\x00\xff\x01\x7f\x7f' * 10000)
    index = self.cmdGetIndex(dataset, min_range_length=256)
    self.assertEqual(index, b'\x00\xff\x01' + b'\x80\x02\xff\x01' * 9999)
    index = self.cmdGetIndex(dataset, min_range_length=257)
    self.assertEqual(index, b'')

    # Ensure Windows doesn't convert '\n' to '\r\n'
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x0a\x0a' * 20)
    index = self.cmdGetIndex(dataset)
    self.assertEqual(index, b'\x0a\x0a' * 20)
    index = self.cmdGetIndex(dataset, min_range_length=12)
    self.assertEqual(index, b'')

if __name__ == '__main__':
  unittest.main()
