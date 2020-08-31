from helper import ExitCode, SystemTest
from tempfile import NamedTemporaryFile
import unittest

class AddTest(SystemTest):

  def test_info(self):
    self.assertInfo('btoep-add', [
      '--dataset', '--index-path', '--lockfile-path',
      '--offset', '--on-conflict', '--source'
    ])

  def test_add(self):
    all_data = ((b'\xaa' * 256) + (b'\xbb' * 256) + (b'\xcc' * 256)) * 3

    # Create a new dataset.
    dataset = self.reserveDataset()
    self.cmd(['btoep-add', '--dataset', dataset, '--offset=512'],
             input = all_data[512:640])
    self.assertEqual(self.readDataset(dataset)[512:640], all_data[512:640])
    self.assertEqual(len(self.readDataset(dataset)), 640)
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\x7f')

    # Add another, separate range.
    self.cmd(['btoep-add', '--dataset', dataset, '--offset=1024'],
             input = all_data[1024:1152])
    self.assertEqual(self.readDataset(dataset)[512:640], all_data[512:640])
    self.assertEqual(self.readDataset(dataset)[1024:1152], all_data[1024:1152])
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\x7f\xff\x02\x7f')

    # Add a range that merges both existing ranges, but does not overlap.
    self.cmd(['btoep-add', '--dataset', dataset, '--offset=640'],
             input = all_data[640:1024])
    self.assertEqual(self.readDataset(dataset)[512:1152], all_data[512:1152])
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\xff\x04')

    # Now add a superset.
    self.cmd(['btoep-add', '--dataset', dataset, '--offset=256'],
             input = all_data[256:1280])
    self.assertEqual(self.readDataset(dataset)[256:1280], all_data[256:1280])
    self.assertEqual(self.readIndex(dataset), b'\x80\x02\xff\x07')

    # Create conflicting data by swapping every single bit.
    conflicting_data = bytes([v ^ 0xff for v in all_data])

    # Adding conflicting data with the default/error behavior should not change
    # existing ranges or the file size, and should not modify the index.
    self.cmd(['btoep-add', '--dataset', dataset, '--offset=0'],
             input = conflicting_data,
             expected_stderr = 'Error: Data conflict\n',
             expected_returncode = ExitCode.APP_ERROR)

    self.cmd(['btoep-add', '--dataset', dataset, '--offset=0', '--on-conflict=error'],
             input = conflicting_data,
             expected_stderr = 'Error: Data conflict\n',
             expected_returncode = ExitCode.APP_ERROR)

    # Ensure dataset and index are unchanged.
    self.assertEqual(self.readDataset(dataset)[256:1280], all_data[256:1280])
    self.assertEqual(len(self.readDataset(dataset)), 1280)
    self.assertEqual(self.readIndex(dataset), b'\x80\x02\xff\x07')

    self.cmd(['btoep-add', '--dataset', dataset, '--offset=0', '--on-conflict=keep'],
             input = conflicting_data)
    self.assertEqual(self.readDataset(dataset)[0:256], conflicting_data[0:256])
    self.assertEqual(self.readDataset(dataset)[256:1280], all_data[256:1280])
    self.assertEqual(self.readDataset(dataset)[1280:], conflicting_data[1280:])
    self.assertEqual(len(self.readDataset(dataset)), len(conflicting_data))
    self.assertEqual(self.readIndex(dataset), b'\x00\xff\x11')

    self.cmd(['btoep-add', '--dataset', dataset, '--offset=0', '--on-conflict=overwrite'],
             input = all_data)
    self.assertEqual(self.readDataset(dataset), all_data)
    self.assertEqual(len(self.readDataset(dataset)), len(all_data))
    self.assertEqual(self.readIndex(dataset), b'\x00\xff\x11')

  def test_add_from_file(self):
    # Create a new dataset from a file.
    path = self.createTempTestFile(b'Hello world\n')
    dataset = self.reserveDataset()
    self.cmd(['btoep-add', '--dataset', dataset, '--source', path, '--offset=0'])
    self.assertEqual(self.readDataset(dataset), b'Hello world\n')
    self.assertEqual(self.readIndex(dataset), b'\x00\x0b')

    # Now do the same with Windows-style line endings.
    path = self.createTempTestFile(b'Hello world\r\n')
    dataset = self.reserveDataset()
    self.cmd(['btoep-add', '--dataset', dataset, '--source', path, '--offset=0'])
    self.assertEqual(self.readDataset(dataset), b'Hello world\r\n')
    self.assertEqual(self.readIndex(dataset), b'\x00\x0c')

if __name__ == '__main__':
  unittest.main()
