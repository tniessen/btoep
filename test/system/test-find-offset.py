from helper import SystemTest
import unittest

NO_RESULT = 1

class FindOffsetTest(SystemTest):

  def findOffset(self, dataset, start_at, stop_at):
    result = self.cmd('btoep-find-offset', '--dataset', dataset,
                      '--start-at', str(start_at), '--stop-at', stop_at)
    self.assertEqual(result.stderr, b'')
    return int(result.stdout)

  def assertNoResult(self, dataset, start_at, stop_at):
    result = self.cmd('btoep-find-offset', '--dataset', dataset, '--start-at',
                      str(start_at), '--stop-at', stop_at, check=False)
    self.assertEqual(result.stdout, b'')
    self.assertEqual(result.stderr, b'')
    self.assertEqual(result.returncode, NO_RESULT)

  def test_empty_index(self):
    # Test an empty dataset with an empty index
    dataset = self.createDataset(b'', b'')
    self.assertNoResult(dataset, 0, 'data')
    self.assertNoResult(dataset, 100, 'data')
    self.assertEqual(self.findOffset(dataset, 0, 'missing'), 0)
    self.assertEqual(self.findOffset(dataset, 100, 'missing'), 100)

  def test_non_empty_index(self):
    # Test a 512 KiB dataset with a non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x00\x00')
    self.assertEqual(self.findOffset(dataset, 0, 'data'), 0)
    self.assertNoResult(dataset, 1, 'data')
    self.assertNoResult(dataset, 100, 'data')
    self.assertEqual(self.findOffset(dataset, 0, 'missing'), 1)
    self.assertEqual(self.findOffset(dataset, 1, 'missing'), 1)
    self.assertEqual(self.findOffset(dataset, 100, 'missing'), 100)
    self.assertEqual(self.findOffset(dataset, 5000000, 'missing'), 5000000)

    # Test a 512 KiB dataset with another non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x81\x01\x7f\x00\x7f')
    self.assertEqual(self.findOffset(dataset, 0, 'data'), 129)
    self.assertEqual(self.findOffset(dataset, 128, 'data'), 129)
    self.assertEqual(self.findOffset(dataset, 256, 'data'), 256)
    self.assertEqual(self.findOffset(dataset, 257, 'data'), 258)
    self.assertEqual(self.findOffset(dataset, 258, 'data'), 258)
    self.assertEqual(self.findOffset(dataset, 385, 'data'), 385)
    self.assertNoResult(dataset, 386, 'data')
    self.assertEqual(self.findOffset(dataset, 0, 'missing'), 0)
    self.assertEqual(self.findOffset(dataset, 128, 'missing'), 128)
    self.assertEqual(self.findOffset(dataset, 129, 'missing'), 257)
    self.assertEqual(self.findOffset(dataset, 256, 'missing'), 257)
    self.assertEqual(self.findOffset(dataset, 257, 'missing'), 257)
    self.assertEqual(self.findOffset(dataset, 258, 'missing'), 386)
    self.assertEqual(self.findOffset(dataset, 385, 'missing'), 386)
    self.assertEqual(self.findOffset(dataset, 10000, 'missing'), 10000)

if __name__ == '__main__':
  unittest.main()
