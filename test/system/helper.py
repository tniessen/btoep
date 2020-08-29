import os
import shutil
import subprocess
import tempfile
import unittest

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

  def cmd(self, *args, **kwargs):
    if not 'check' in kwargs:
      kwargs['check'] = True
    return subprocess.run(args, capture_output=True, timeout=10, **kwargs)
