from helper import ExitCode, SystemTest
import unittest

class FindOffsetTest(SystemTest):

  def test_info(self):
    self.assertInfo('btoep-find-offset', [
      '--dataset', '--index-path', '--lockfile-path',
      '--start-at', '--stop-at'
    ])

  def findOffset(self, dataset, start_at, stop_at):
    output = self.cmd_stdout(['btoep-find-offset', '--dataset', dataset,
                              '--start-at', str(start_at),
                              '--stop-at', stop_at],
                             text=True)
    return int(output)

  def assertNoResult(self, dataset, start_at, stop_at):
    self.cmd(['btoep-find-offset', '--dataset', dataset, '--start-at',
                      str(start_at), '--stop-at', stop_at],
             expected_returncode = ExitCode.NO_RESULT)

  def test_empty_index(self):
    # Test an empty dataset with an empty index
    dataset = self.createDataset(b'', b'')
    self.assertNoResult(dataset, 0, 'data')
    self.assertNoResult(dataset, 100, 'data')
    self.assertEqual(self.findOffset(dataset, 0, 'no-data'), 0)
    self.assertEqual(self.findOffset(dataset, 100, 'no-data'), 100)

  def test_non_empty_index(self):
    # Test a 512 KiB dataset with a non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x00\x00')
    self.assertEqual(self.findOffset(dataset, 0, 'data'), 0)
    self.assertNoResult(dataset, 1, 'data')
    self.assertNoResult(dataset, 100, 'data')
    self.assertEqual(self.findOffset(dataset, 0, 'no-data'), 1)
    self.assertEqual(self.findOffset(dataset, 1, 'no-data'), 1)
    self.assertEqual(self.findOffset(dataset, 100, 'no-data'), 100)
    self.assertEqual(self.findOffset(dataset, 5000000, 'no-data'), 5000000)

    # Test a 512 KiB dataset with another non-empty index
    dataset = self.createDataset(b'\x00' * 1024 * 512, b'\x81\x01\x7f\x00\x7f')
    self.assertEqual(self.findOffset(dataset, 0, 'data'), 129)
    self.assertEqual(self.findOffset(dataset, 128, 'data'), 129)
    self.assertEqual(self.findOffset(dataset, 256, 'data'), 256)
    self.assertEqual(self.findOffset(dataset, 257, 'data'), 258)
    self.assertEqual(self.findOffset(dataset, 258, 'data'), 258)
    self.assertEqual(self.findOffset(dataset, 385, 'data'), 385)
    self.assertNoResult(dataset, 386, 'data')
    self.assertEqual(self.findOffset(dataset, 0, 'no-data'), 0)
    self.assertEqual(self.findOffset(dataset, 128, 'no-data'), 128)
    self.assertEqual(self.findOffset(dataset, 129, 'no-data'), 257)
    self.assertEqual(self.findOffset(dataset, 256, 'no-data'), 257)
    self.assertEqual(self.findOffset(dataset, 257, 'no-data'), 257)
    self.assertEqual(self.findOffset(dataset, 258, 'no-data'), 386)
    self.assertEqual(self.findOffset(dataset, 385, 'no-data'), 386)
    self.assertEqual(self.findOffset(dataset, 10000, 'no-data'), 10000)

  def test_fs_error(self):
    # Test that the command fails if the dataset does not exist.
    dataset = self.reserveDataset()
    stderr = self.cmd_stderr(['btoep-find-offset', '--dataset', dataset,
                              '--start-at=0', '--stop-at=data'],
                             expected_returncode = ExitCode.APP_ERROR)
    self.assertTrue(stderr.startswith('Error: System input/output error: '))

if __name__ == '__main__':
  unittest.main()
