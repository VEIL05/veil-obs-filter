"""Valid tiny ONNX graph that loads but rejects full-size frames during Run.

Test dependency: onnx==1.19.0. This fixture is for private payload copies only;
never use it in the installed plugin. See background_smoke --inference-failure.
"""
from pathlib import Path
import sys
import onnx
from onnx import TensorProto, helper

output = Path(sys.argv[1]).resolve()
source = helper.make_tensor_value_info('src', TensorProto.FLOAT, [1, 3, 32, 32])
states = [helper.make_tensor_value_info(f'r{i}i', TensorProto.FLOAT, [1, 1, 1, 1])
          for i in range(1, 5)]
ratio = helper.make_tensor_value_info('downsample_ratio', TensorProto.FLOAT, [1])
names = [('src', 'fgr', [1, 3, 32, 32]), ('r1i', 'pha', [1, 1, 1, 1])]
names += [(f'r{i}i', f'r{i}o', [1, 1, 1, 1]) for i in range(1, 5)]
nodes = [helper.make_node('Identity', [src], [dst]) for src, dst, _ in names]
outputs = [helper.make_tensor_value_info(dst, TensorProto.FLOAT, shape)
           for _, dst, shape in names]
graph = helper.make_graph(nodes, 'VEIL deliberate inference failure',
                          [source, *states, ratio], outputs)
model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 17)],
                          producer_name='VEIL public-fixture test')
model.ir_version = 9
onnx.checker.check_model(model)
output.parent.mkdir(parents=True, exist_ok=True)
onnx.save(model, output)
print(f'PASS valid six-input/six-output test model written: {output.name}')
