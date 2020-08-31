import enum
import io
import os
import shutil
import subprocess
import tempfile
import unittest

class ExitCode(enum.Enum):
  SUCCESS = 0
  NO_RESULT = 1
  USAGE_ERROR = 2
  APP_ERROR = 3

def tostring(b):
  return io.StringIO(b.decode(), None).getvalue()

class SystemTest(unittest.TestCase):

  def setUp(self):
    self.dir = tempfile.mkdtemp()
    self.n_datasets = 0

  def tearDown(self):
    shutil.rmtree(self.dir)

  def reserveDataset(self):
    path = os.path.join(self.dir, 'dataset_' + str(self.n_datasets))
    self.n_datasets += 1
    return path

  def createTempTestFile(self, data):
    with tempfile.NamedTemporaryFile(delete=False, dir=self.dir) as file:
      file.write(data)
      return file.name

  def createDataset(self, data, index_data):
    path = self.reserveDataset()
    with open(path, 'wb') as dataset:
      dataset.write(data)
    with open(path + '.idx', 'wb') as index:
      index.write(index_data)
    return path

  def readDataset(self, path):
    with open(path, 'rb') as dataset:
      return dataset.read()

  def readIndex(self, path):
    with open(path + '.idx', 'rb') as dataset:
      return dataset.read()

  def assertOutputIsEqual(self, actual, expected):
    if isinstance(expected, str):
      self.assertEqual(tostring(actual), expected)
    else:
      self.assertEqual(actual, expected)

  def cmd(self, args,
          expected_returncode=ExitCode.SUCCESS,
          expected_stdout=b'',
          expected_stderr=b'',
          **kwargs):
    result = subprocess.run(args, capture_output=True, timeout=10, **kwargs)
    self.assertEqual(result.returncode, expected_returncode.value)
    if expected_stdout is not None:
      self.assertOutputIsEqual(result.stdout, expected_stdout)
    if expected_stderr is not None:
      self.assertOutputIsEqual(result.stderr, expected_stderr)
    return result

  def cmd_stdout(self, args, text=False, **kwargs):
    result = self.cmd(args, expected_stdout=None, **kwargs)
    return (tostring(result.stdout) if text else result.stdout)

  def assertInfo(self, cmd, options):
    version = self.cmd_stdout([cmd, '--version'], text=True)
    self.assertTrue(version.startswith(cmd + ' '))

    help = self.cmd_stdout([cmd, '--help'], text=True)
    self.assertTrue(help.startswith('Usage: ' + cmd + ' [options]\n'))
    for option in options + ['--version', '--help']:
      self.assertIn('\n' + option, help)
    # By convention, the information should not exceed 80 columns.
    for line in help.splitlines():
      self.assertLessEqual(len(line), 80)
