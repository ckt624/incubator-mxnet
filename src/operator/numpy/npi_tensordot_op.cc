/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file npi_tensordot.cc
 * \brief CPU Implementation of numpy-compatible tensordot
 */

#include "npi_tensordot_op-inl.h"

namespace mxnet {
namespace op {

/** 
 * Recovers 0 size shapes.
*/
inline void recover0Size(TShape* shape) {
  for (auto& i: *shape) {
    if (i == -1) {
      i = 0;
    }
  }
}

inline bool TensordotOpShape(const nnvm::NodeAttrs& attrs,
                          mxnet::ShapeVector *in_attrs,
                          mxnet::ShapeVector *out_attrs) {
  CHECK_EQ(in_attrs->size(), 2U);
  CHECK_EQ(out_attrs->size(), 1U);

  recover0Size(&in_attrs->at(0));
  recover0Size(&in_attrs->at(1));
  const mxnet::TShape& a_shape = in_attrs->at(0);
  const mxnet::TShape& b_shape = in_attrs->at(1);

  if (!ndim_is_known(a_shape) || !ndim_is_known(b_shape)) {
    return false;
  }

  const TensordotParam& param = nnvm::get<TensordotParam>(attrs.parsed);      
  const Tuple<int>& a_axes_summed = param.a_axes_summed;
  const Tuple<int>& b_axes_summed = param.b_axes_summed;

  Tuple<int> a_axes_remained;
  Tuple<int> b_axes_remained;
  Tuple<int> a_axes;
  Tuple<int> b_axes;
  getReorderedAxes(a_axes_summed, a_axes_remained, a_axes, b_axes_summed, b_axes_remained, 
    b_axes, a_shape, b_shape);

  CHECK_EQ(a_axes_summed.ndim(), b_axes_summed.ndim());

  mxnet::TShape out_shape(a_axes_remained.ndim() + b_axes_remained.ndim(), -1);
  for (int i = 0; i < a_axes_remained.ndim(); i++) {
    out_shape[i] = a_shape[a_axes_remained[i]];
  }
  for (int i = 0; i < b_axes_remained.ndim(); i++) {
    out_shape[a_axes_remained.ndim() + i] = b_shape[b_axes_remained[i]];
  }
  SHAPE_ASSIGN_CHECK(*out_attrs, 0, out_shape);

  mxnet::TShape tem_shape1(a_axes.ndim(), -1);
  for (int i = 0; i < a_axes_remained.ndim(); i++) {
    tem_shape1[a_axes_remained[i]] = out_shape[i];
  }
  for (int i = 0; i < a_axes_summed.ndim(); i++) {
    tem_shape1[a_axes_summed[i]] = b_shape[b_axes_summed[i]];
  }
  SHAPE_ASSIGN_CHECK(*in_attrs, 0, tem_shape1);

  mxnet::TShape tem_shape2(b_axes.ndim(), -1);
  for (int i = 0; i < b_axes_remained.ndim(); i++) {
    tem_shape2[b_axes_remained[i]] = out_shape[a_axes_remained.ndim() + i];
  }
  for (int i = 0; i < b_axes_summed.ndim(); i++) {
    tem_shape2[b_axes_summed[i]] = a_shape[a_axes_summed[i]];
  }
  SHAPE_ASSIGN_CHECK(*in_attrs, 1, tem_shape2);

  return shape_is_known(*in_attrs) && shape_is_known(*out_attrs); 
}

DMLC_REGISTER_PARAMETER(TensordotParam);                                           

NNVM_REGISTER_OP(tensordot)
.add_alias("_npi_tensordot")                                                        
.describe(R"code(This operators implements the numpy-compatible tensordot function                 
)code" ADD_FILELINE) // description. 
.set_attr_parser(mxnet::op::ParamParser<TensordotParam>)  
.set_num_inputs(2)
.set_num_outputs(1)
.set_attr<nnvm::FListInputNames>("FListInputNames",
  [](const NodeAttrs& attrs) {
    return std::vector<std::string>{"a", "b"};
  })
.set_attr<mxnet::FInferShape>("FInferShape", TensordotOpShape)
.set_attr<nnvm::FInferType>("FInferType", mxnet::op::ElemwiseType<2, 1>)
.set_attr<FResourceRequest>("FResourceRequest",
  [](const NodeAttrs& attrs) {
    return std::vector<ResourceRequest>{ResourceRequest::kTempSpace};
  })
.set_attr<FCompute>("FCompute<cpu>", TensordotOpForward<cpu>)
.set_attr<nnvm::FGradient>("FGradient", mxnet::op::ElemwiseGradUseIn{"_backward_tensordot"})
.add_argument("a", "NDArray-or-Symbol", "First input")
.add_argument("b", "NDArray-or-Symbol", "Second input")
.add_arguments(TensordotParam::__FIELDS__()); // abc  

NNVM_REGISTER_OP(_backward_tensordot)
.set_attr_parser(mxnet::op::ParamParser<TensordotParam>) 
.set_num_inputs(3)
.set_num_outputs(2)
.set_attr<nnvm::TIsBackward>("TIsBackward", true)
.set_attr<FResourceRequest>("FResourceRequest",
  [](const NodeAttrs& attrs) {
    return std::vector<ResourceRequest>{ResourceRequest::kTempSpace};
  })
.set_attr<FCompute>("FCompute<cpu>", TensordotOpBackward<cpu>);

}  // namespace op
}  // namespace mxnet
