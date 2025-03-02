/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef ONEFLOW_CORE_FRAMEWORK_TENSOR_H_
#define ONEFLOW_CORE_FRAMEWORK_TENSOR_H_

#include "oneflow/core/common/data_type.h"
#include "oneflow/core/common/data_type.cfg.h"
#include "oneflow/core/common/shape_view.h"
#include "oneflow/core/common/shape.h"
#include "oneflow/core/memory/memory_case.pb.h"
#include "oneflow/core/framework/tensor.h"
#include "oneflow/core/framework/tensor_impl.h"
#include "oneflow/core/framework/transport_token.h"
#include "oneflow/core/common/error.h"

namespace oneflow {

namespace cfg {
class NdSbp;
}
class Device;

namespace one {

class FunctionNode;

class ConsistentTensor;
class MirroredTensor;

class Tensor {
 public:
  virtual ~Tensor() = default;

  // Getters
  int64_t dim(int64_t index) const { return shape()->At(index); }
  int64_t nelement() const { return shape()->elem_cnt(); }
  int64_t ndim() const { return shape()->NumAxes(); }

  virtual const std::shared_ptr<const Shape>& shape() const = 0;
  virtual Symbol<DType> dtype() const = 0;
  virtual Maybe<TransportToken> transport_token() const = 0;
  virtual Maybe<Symbol<cfg::NdSbp>> nd_sbp() const = 0;
  virtual Maybe<Symbol<ParallelDesc>> parallel_desc() const = 0;
  virtual Maybe<Symbol<Device>> device() const = 0;
  virtual Maybe<Symbol<Device>*> mut_device() = 0;
  virtual bool is_cuda() const = 0;
  virtual bool is_consistent() const = 0;
  virtual bool is_local() const { return !is_consistent(); }
  virtual bool is_lazy() const = 0;
  virtual bool is_eager() const { return !is_lazy(); }
  virtual const TensorMeta& tensor_meta() const = 0;
  virtual Maybe<Symbol<ConsistentTensorMeta>> consistent_tensor_meta() const { OF_UNIMPLEMENTED(); }

  // Getters valid only for EagerMirroredTensor
  virtual Maybe<EagerMirroredTensorImpl*> mut_eager_mirrored_tensor_impl() { OF_UNIMPLEMENTED(); }
  virtual Maybe<vm::EagerBlobObject> eager_blob_object() const = 0;
  virtual Maybe<LocalDepObject*> compute_local_dep_object() const = 0;
  virtual Maybe<bool> has_eager_blob_object() const = 0;
  virtual Maybe<TensorStorage> tensor_storage() const { OF_UNIMPLEMENTED(); }
  virtual Maybe<const Stride> stride() const { OF_UNIMPLEMENTED(); }
  virtual Maybe<int64_t> storage_offset() const { OF_UNIMPLEMENTED(); }

  // Getters/Setters valid only for EagerConsistentTensor
  virtual Maybe<const Optional<Symbol<cfg::NdSbp>>&> consumer_nd_sbp_constraint() const {
    OF_UNIMPLEMENTED();
  }
  virtual Maybe<MirroredTensor> cur_rank_phy_tensor() const { OF_UNIMPLEMENTED(); }
  virtual Maybe<void> set_consumer_nd_sbp_constraint(Symbol<cfg::NdSbp> val) { OF_UNIMPLEMENTED(); }

  // Getters for autograd
  virtual bool requires_grad() const = 0;
  virtual bool is_leaf() const = 0;
  virtual bool retain_grad() const = 0;
  virtual std::shared_ptr<const FunctionNode> grad_fn_node() const = 0;
  virtual Maybe<Tensor> acc_grad() const = 0;
  virtual Maybe<TensorArg> current_grad() const = 0;
  virtual Maybe<Tensor> detach() const = 0;
  virtual Maybe<Tensor> clone() const = 0;
  virtual std::shared_ptr<Tensor> data() const = 0;

  // Setters for autograd
  virtual void set_requires_grad(bool requires_grad) = 0;
  virtual Maybe<void> set_retain_grad(bool retain_grad) = 0;
  virtual void set_grad_fn_node(const std::shared_ptr<FunctionNode>& grad_fn_node) = 0;
  virtual const std::shared_ptr<FunctionNode>& mut_grad_fn_node() = 0;
  virtual Maybe<void> set_acc_grad(const std::shared_ptr<Tensor>& grad) = 0;
  virtual Maybe<Tensor> mut_acc_grad() = 0;
  virtual void set_is_leaf(bool is_leaf) = 0;
  virtual std::shared_ptr<AutogradMeta> mut_autograd_meta() = 0;
  virtual bool has_autograd_meta() const = 0;
  virtual void set_autograd_meta(const std::shared_ptr<AutogradMeta>& autograd_meta) = 0;

  virtual user_op::TensorDesc* mut_tensor_meta() = 0;

  virtual Maybe<MirroredTensor> AsMirroredTensor() = 0;
  virtual Maybe<ConsistentTensor> AsConsistentTensor() = 0;

 protected:
  Tensor() = default;
};

class StaticZerosTensor final : public Tensor {
 public:
  static Maybe<StaticZerosTensor> MakeTensor(const std::shared_ptr<const Shape>& shape,
                                             DataType dtype, Symbol<Device> device) {
    return std::shared_ptr<StaticZerosTensor>(new StaticZerosTensor(shape, dtype, device));
  }
  // Getters
  const std::shared_ptr<const Shape>& shape() const { return shape_; }
  Symbol<DType> dtype() const { return CHECK_JUST(DType::Get(dtype_)); }
  Maybe<TransportToken> transport_token() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<Symbol<cfg::NdSbp>> nd_sbp() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<Symbol<ParallelDesc>> parallel_desc() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<Symbol<Device>> device() const { return device_; }
  Maybe<Symbol<Device>*> mut_device() { RETURN_ERROR_WITH_BUG_PROMPT(); }
  bool is_cuda() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return false;
  }
  bool is_consistent() const { return false; }
  bool is_local() const { return !is_consistent(); }
  bool is_lazy() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return false;
  }
  bool is_eager() const { return !is_lazy(); }
  const TensorMeta& tensor_meta() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return *(TensorMeta*)nullptr;
  }
  Maybe<Symbol<ConsistentTensorMeta>> consistent_tensor_meta() const {
    RETURN_ERROR_WITH_BUG_PROMPT();
  }

  // Getters valid only for EagerMirroredTensor
  Maybe<EagerMirroredTensorImpl*> mut_eager_mirrored_tensor_impl() {
    RETURN_ERROR_WITH_BUG_PROMPT();
  }
  Maybe<vm::EagerBlobObject> eager_blob_object() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<LocalDepObject*> compute_local_dep_object() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<bool> has_eager_blob_object() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<TensorStorage> tensor_storage() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<const Stride> stride() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<int64_t> storage_offset() const { RETURN_ERROR_WITH_BUG_PROMPT(); }

  // Getters/Setters valid only for EagerConsistentTensor
  Maybe<const Optional<Symbol<cfg::NdSbp>>&> consumer_nd_sbp_constraint() const {
    RETURN_ERROR_WITH_BUG_PROMPT();
  }
  Maybe<MirroredTensor> cur_rank_phy_tensor() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<void> set_consumer_nd_sbp_constraint(Symbol<cfg::NdSbp> val) {
    RETURN_ERROR_WITH_BUG_PROMPT();
  }

  // Getters for autograd
  bool requires_grad() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return false;
  }
  bool is_leaf() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return false;
  }
  bool retain_grad() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return false;
  }
  std::shared_ptr<const FunctionNode> grad_fn_node() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return nullptr;
  }
  Maybe<Tensor> acc_grad() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<TensorArg> current_grad() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<Tensor> detach() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<Tensor> clone() const { RETURN_ERROR_WITH_BUG_PROMPT(); }
  std::shared_ptr<Tensor> data() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return nullptr;
  }

  // Setters for autograd
  void set_requires_grad(bool requires_grad) { PRINT_BUG_PROMPT_AND_ABORT(); }
  Maybe<void> set_retain_grad(bool retain_grad) { RETURN_ERROR_WITH_BUG_PROMPT(); }
  void set_grad_fn_node(const std::shared_ptr<FunctionNode>& grad_fn_node) {
    PRINT_BUG_PROMPT_AND_ABORT();
  }
  const std::shared_ptr<FunctionNode>& mut_grad_fn_node() {
    PRINT_BUG_PROMPT_AND_ABORT();
    return *(std::shared_ptr<FunctionNode>*)nullptr;
  }
  Maybe<void> set_acc_grad(const std::shared_ptr<Tensor>& grad) { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<Tensor> mut_acc_grad() { RETURN_ERROR_WITH_BUG_PROMPT(); }
  void set_is_leaf(bool is_leaf) { PRINT_BUG_PROMPT_AND_ABORT(); }
  std::shared_ptr<AutogradMeta> mut_autograd_meta() {
    PRINT_BUG_PROMPT_AND_ABORT();
    return nullptr;
  }
  bool has_autograd_meta() const {
    PRINT_BUG_PROMPT_AND_ABORT();
    return false;
  }
  void set_autograd_meta(const std::shared_ptr<AutogradMeta>& autograd_meta) {
    PRINT_BUG_PROMPT_AND_ABORT();
  }

  user_op::TensorDesc* mut_tensor_meta() {
    PRINT_BUG_PROMPT_AND_ABORT();
    return nullptr;
  }

  Maybe<MirroredTensor> AsMirroredTensor();
  Maybe<ConsistentTensor> AsConsistentTensor() { RETURN_ERROR_WITH_BUG_PROMPT(); }

 private:
  StaticZerosTensor(const std::shared_ptr<const Shape>& shape, DataType dtype,
                    Symbol<Device> device)
      : shape_(shape), dtype_(dtype), device_(device) {}
  const std::shared_ptr<const Shape> shape_;
  DataType dtype_;
  Symbol<Device> device_;
};

template<typename DerivedT>
class TensorIf : public Tensor {
 public:
  virtual ~TensorIf() = default;

  // Getters for autograd
  // acc_grad is tensor's accumulated grad in more than once backward operation,
  // and current_grad is temporary grad to shared data with different FunctionNode
  std::shared_ptr<const FunctionNode> grad_fn_node() const override { return grad_fn_node_; }

  // Setters for autograd
  void set_grad_fn_node(const std::shared_ptr<FunctionNode>& grad_fn_node) override {
    grad_fn_node_ = grad_fn_node;
  }
  const std::shared_ptr<FunctionNode>& mut_grad_fn_node() override { return grad_fn_node_; }

 protected:
  TensorIf() = default;
  std::shared_ptr<FunctionNode> grad_fn_node_;
};

class Parameter final : public TensorIf<Parameter> {
 public:
  Parameter(std::shared_ptr<Tensor> tensor, bool requires_grad) {
    while (auto parameter = std::dynamic_pointer_cast<Parameter>(tensor)) {
      tensor = parameter->tensor_;
    }
    this->tensor_ = std::move(tensor);
    // TODO: in `y = flow.nn.Parameter(x)`, y should have its own "requires_grad" field
    // (align with PyTorch) instead of sharing it with x
    this->tensor_->set_requires_grad(requires_grad);
  }

  const std::shared_ptr<const Shape>& shape() const override { return tensor_->shape(); }
  Symbol<DType> dtype() const override { return tensor_->dtype(); }
  Maybe<Symbol<cfg::NdSbp>> nd_sbp() const override { return tensor_->nd_sbp(); }
  Maybe<Symbol<ParallelDesc>> parallel_desc() const override { return tensor_->parallel_desc(); }
  Maybe<Symbol<Device>> device() const override { return tensor_->device(); }
  Maybe<Symbol<Device>*> mut_device() override { return tensor_->mut_device(); }
  bool is_cuda() const override { return tensor_->is_cuda(); }
  bool is_consistent() const override { return tensor_->is_consistent(); }
  bool is_local() const override { return tensor_->is_local(); }
  bool is_lazy() const override { return tensor_->is_lazy(); }
  bool is_eager() const override { return tensor_->is_eager(); }
  const TensorMeta& tensor_meta() const override { return tensor_->tensor_meta(); }
  Maybe<Symbol<ConsistentTensorMeta>> consistent_tensor_meta() const override {
    return tensor_->consistent_tensor_meta();
  }

  Maybe<EagerMirroredTensorImpl*> mut_eager_mirrored_tensor_impl() override {
    return tensor_->mut_eager_mirrored_tensor_impl();
  }
  Maybe<vm::EagerBlobObject> eager_blob_object() const override {
    return tensor_->eager_blob_object();
  }
  Maybe<LocalDepObject*> compute_local_dep_object() const override {
    return tensor_->compute_local_dep_object();
  }
  Maybe<bool> has_eager_blob_object() const override { return tensor_->has_eager_blob_object(); }
  Maybe<TensorStorage> tensor_storage() const override { return tensor_->tensor_storage(); }
  Maybe<const Stride> stride() const override { return tensor_->stride(); }
  Maybe<int64_t> storage_offset() const override { return tensor_->storage_offset(); }

  Maybe<const Optional<Symbol<cfg::NdSbp>>&> consumer_nd_sbp_constraint() const override {
    return tensor_->consumer_nd_sbp_constraint();
  }
  Maybe<TransportToken> transport_token() const override { return tensor_->transport_token(); }
  Maybe<MirroredTensor> cur_rank_phy_tensor() const override {
    return tensor_->cur_rank_phy_tensor();
  }
  Maybe<void> set_consumer_nd_sbp_constraint(Symbol<cfg::NdSbp> val) override {
    return tensor_->set_consumer_nd_sbp_constraint(val);
  }

  bool requires_grad() const override { return tensor_->requires_grad(); }
  bool is_leaf() const override { return true; }
  bool retain_grad() const override { return tensor_->retain_grad(); }
  Maybe<Tensor> acc_grad() const override { return tensor_->acc_grad(); }
  Maybe<TensorArg> current_grad() const override { return tensor_->current_grad(); }
  Maybe<Tensor> detach() const override { return tensor_->detach(); }
  Maybe<Tensor> clone() const override { return tensor_->clone(); }
  std::shared_ptr<Tensor> data() const override { return tensor_->data(); }

  void set_requires_grad(bool requires_grad) override {
    return tensor_->set_requires_grad(requires_grad);
  }
  Maybe<void> set_retain_grad(bool retain_grad) override {
    return tensor_->set_retain_grad(retain_grad);
  }
  Maybe<void> set_acc_grad(const std::shared_ptr<Tensor>& grad) override {
    return tensor_->set_acc_grad(grad);
  }
  Maybe<Tensor> mut_acc_grad() override { return tensor_->mut_acc_grad(); }
  void set_is_leaf(bool is_leaf) override { return tensor_->set_is_leaf(is_leaf); }
  std::shared_ptr<AutogradMeta> mut_autograd_meta() override {
    return tensor_->mut_autograd_meta();
  }
  bool has_autograd_meta() const override { return tensor_->has_autograd_meta(); }
  void set_autograd_meta(const std::shared_ptr<AutogradMeta>& autograd_meta) override {
    return tensor_->set_autograd_meta(autograd_meta);
  }

  user_op::TensorDesc* mut_tensor_meta() override { return tensor_->mut_tensor_meta(); }

  Maybe<MirroredTensor> AsMirroredTensor() override {
    if (const auto& mirrored_tensor = std::dynamic_pointer_cast<MirroredTensor>(tensor_)) {
      return mirrored_tensor;
    }
    OF_RUNTIME_ERROR() << "Parameter Tensor has no AsMirroredTensor property";
  }

  Maybe<ConsistentTensor> AsConsistentTensor() override {
    if (const auto& consistent_tensor = std::dynamic_pointer_cast<ConsistentTensor>(tensor_)) {
      return consistent_tensor;
    }
    RETURN_ERROR_WITH_BUG_PROMPT();
  }

 private:
  std::shared_ptr<Tensor> tensor_;
};

class MirroredTensor final : public TensorIf<MirroredTensor>,
                             public std::enable_shared_from_this<MirroredTensor> {
 public:
  OF_DISALLOW_COPY_AND_MOVE(MirroredTensor);
  MirroredTensor() = default;
  explicit MirroredTensor(const std::shared_ptr<MirroredTensorImpl>& impl) { impl_ = impl; }
  ~MirroredTensor() override = default;

  // Getters
  const std::shared_ptr<const Shape>& shape() const override { return impl_->shape(); }
  Symbol<DType> dtype() const override { return CHECK_JUST(DType::Get(impl_->dtype())); }
  Maybe<TransportToken> transport_token() const override {
    OF_RUNTIME_ERROR() << "Only consistent tensors have 'consistent_id', Consistent id is used to "
                          "synchronize rank";
  }
  Maybe<Symbol<cfg::NdSbp>> nd_sbp() const override {
    OF_RUNTIME_ERROR()
        << "Local tensor has no sbp property. "
           "sbp is the description in the oneflow distributed case, you can refer to "
           "https://docs.oneflow.org/master/basics_topics/essentials_of_oneflow.html; "
           "For example, create a consistent tensor like this : 'x = flow.tensor((2,3, "
           "placement=flow.placement(\"cuda\", {0: 0}), sbp=flow.sbp.broadcast))', then 'x.sbp' is "
           "'flow.sbp.broadcast'";
  }
  Maybe<Symbol<ParallelDesc>> parallel_desc() const override {
    OF_RUNTIME_ERROR() << "Only consistent tensors have 'placement'. Placement is used to describe "
                          "the distribution of consistent tensor in multiple GPUs. Please use "
                          "'.device' for local tensors.";
  }
  Maybe<Symbol<Device>> device() const override { return impl_->device(); }
  Maybe<Symbol<Device>*> mut_device() override { return impl_->mut_device(); }
  bool is_lazy() const override { return impl_->is_lazy(); }
  bool is_consistent() const override { return false; }
  bool is_cuda() const override;
  std::shared_ptr<Tensor> data() const override;
  const TensorMeta& tensor_meta() const override { return *impl_->tensor_meta(); }

  // Getters valid only for EagerMirroredTensor
  Maybe<vm::EagerBlobObject> eager_blob_object() const override {
    return impl_->eager_blob_object();
  }
  Maybe<LocalDepObject*> compute_local_dep_object() const override {
    return impl_->compute_local_dep_object();
  }
  Maybe<TensorStorage> tensor_storage() const override { return impl_->tensor_storage(); }
  Maybe<bool> has_eager_blob_object() const override { return impl_->has_eager_blob_object(); }
  Maybe<const Stride> stride() const override { return impl_->stride(); }
  Maybe<int64_t> storage_offset() const override { return impl_->storage_offset(); }

  // Getters for autograd
  Maybe<Tensor> acc_grad() const override { return impl_->acc_grad(); }
  Maybe<TensorArg> current_grad() const override { return impl_->current_grad(); }
  bool requires_grad() const override { return impl_->requires_grad(); }
  bool is_leaf() const override { return impl_->is_leaf(); }
  bool retain_grad() const override { return impl_->retain_grad(); }
  bool has_autograd_meta() const override { return impl_->has_autograd_meta(); }

  // Setters for autograd
  Maybe<void> set_acc_grad(const std::shared_ptr<Tensor>& grad) override {
    return impl_->set_acc_grad(grad);
  }
  void set_requires_grad(bool requires_grad) override { impl_->set_requires_grad(requires_grad); }
  Maybe<void> set_retain_grad(bool retain_grad) override {
    return impl_->set_retain_grad(retain_grad);
  }
  Maybe<Tensor> mut_acc_grad() override { return impl_->mut_acc_grad(); }
  void set_is_leaf(bool is_leaf) override { impl_->set_is_leaf(is_leaf); }
  std::shared_ptr<AutogradMeta> mut_autograd_meta() override { return impl_->mut_autograd_meta(); }
  void set_autograd_meta(const std::shared_ptr<AutogradMeta>& autograd_meta) override {
    impl_->set_autograd_meta(autograd_meta);
  }

  // Operators for tensor
  Maybe<Tensor> detach() const override;
  Maybe<Tensor> clone() const override;

  static Maybe<MirroredTensor> MakeTensor(const std::shared_ptr<const Shape>& shape, DataType dtype,
                                          const Symbol<Device>& device, bool is_lazy,
                                          bool requires_grad, bool is_leaf);
  MirroredTensorImpl* mut_impl() { return impl_.get(); }
  Maybe<EagerMirroredTensorImpl*> mut_eager_mirrored_tensor_impl() override {
    return impl_->mut_eager_mirrored_tensor_impl();
  }
  user_op::TensorDesc* mut_tensor_meta() override { return impl_->mut_tensor_meta(); }

  Maybe<MirroredTensor> AsMirroredTensor() override { return shared_from_this(); }
  Maybe<ConsistentTensor> AsConsistentTensor() override { RETURN_ERROR_WITH_BUG_PROMPT(); }

 private:
  std::shared_ptr<MirroredTensorImpl> impl_;
};

class ConsistentTensor final : public TensorIf<ConsistentTensor>,
                               public std::enable_shared_from_this<ConsistentTensor> {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ConsistentTensor);
  ConsistentTensor() = default;
  explicit ConsistentTensor(const std::shared_ptr<ConsistentTensorImpl>& impl) { impl_ = impl; }
  ~ConsistentTensor() override = default;

  // Getters
  const std::shared_ptr<const Shape>& shape() const override { return impl_->shape(); }
  Symbol<DType> dtype() const override { return CHECK_JUST(DType::Get(impl_->dtype())); }
  Maybe<TransportToken> transport_token() const override { return impl_->transport_token(); }
  Maybe<Symbol<cfg::NdSbp>> nd_sbp() const override { return impl_->nd_sbp(); }
  Maybe<Symbol<ParallelDesc>> parallel_desc() const override { return impl_->parallel_desc(); }
  Maybe<Symbol<Device>> device() const override {
    OF_RUNTIME_ERROR() << "Only local tensors have 'device'. Please use "
                          "'.placement' for consistent tensors.";
  }
  Maybe<Symbol<Device>*> mut_device() override {
    OF_RUNTIME_ERROR() << "ConsistentTensor has no mut_device property";
  }
  bool is_lazy() const override { return impl_->is_lazy(); }
  bool is_consistent() const override { return true; }
  Maybe<const Optional<Symbol<cfg::NdSbp>>&> consumer_nd_sbp_constraint() const override {
    return impl_->consumer_nd_sbp_constraint();
  }
  Maybe<MirroredTensor> cur_rank_phy_tensor() const override {
    return impl_->cur_rank_phy_tensor();
  }
  bool is_cuda() const override;
  std::shared_ptr<Tensor> data() const override;

  // Getters valid only for EagerMirroredTensor
  Maybe<vm::EagerBlobObject> eager_blob_object() const override {
    return impl_->eager_blob_object();
  }
  Maybe<LocalDepObject*> compute_local_dep_object() const override {
    return impl_->compute_local_dep_object();
  }
  const TensorMeta& tensor_meta() const override { return *impl_->tensor_meta(); }
  Maybe<TensorStorage> tensor_storage() const override { return impl_->tensor_storage(); }
  Maybe<bool> has_eager_blob_object() const override { return impl_->has_eager_blob_object(); }

  // Setters
  Maybe<void> set_consumer_nd_sbp_constraint(Symbol<cfg::NdSbp> val) override {
    impl_->set_consumer_nd_sbp_constraint(val);
    return Maybe<void>::Ok();
  }

  // Getters for autograd
  Maybe<Tensor> acc_grad() const override { return impl_->acc_grad(); }
  Maybe<TensorArg> current_grad() const override { return impl_->current_grad(); }
  bool requires_grad() const override { return impl_->requires_grad(); }
  bool is_leaf() const override { return impl_->is_leaf(); }
  bool retain_grad() const override { return impl_->retain_grad(); }
  bool has_autograd_meta() const override { return impl_->has_autograd_meta(); }

  // Setters for autograd
  Maybe<void> set_acc_grad(const std::shared_ptr<Tensor>& grad) override {
    return impl_->set_acc_grad(grad);
  }
  Maybe<Tensor> mut_acc_grad() override { return impl_->mut_acc_grad(); }
  void set_requires_grad(bool requires_grad) override { impl_->set_requires_grad(requires_grad); }
  Maybe<void> set_retain_grad(bool retain_grad) override {
    return impl_->set_retain_grad(retain_grad);
  }
  void set_is_leaf(bool is_leaf) override { impl_->set_is_leaf(is_leaf); }
  std::shared_ptr<AutogradMeta> mut_autograd_meta() override { return impl_->mut_autograd_meta(); }
  void set_autograd_meta(const std::shared_ptr<AutogradMeta>& autograd_meta) override {
    impl_->set_autograd_meta(autograd_meta);
  }

  // Operators for tensor
  Maybe<Tensor> detach() const override;
  Maybe<Tensor> clone() const override { return Error::UnimplementedError(); }

  static Maybe<ConsistentTensor> MakeTensor(const std::shared_ptr<const Shape>& shape,
                                            DataType dtype, Symbol<cfg::NdSbp> nd_sbp,
                                            Symbol<ParallelDesc> parallel_desc, bool is_lazy,
                                            bool requires_grad, bool is_leaf);

  ConsistentTensorImpl* mut_impl() { return impl_.get(); }

  Maybe<Symbol<ConsistentTensorMeta>> consistent_tensor_meta() const override {
    return impl_->tensor_meta();
  }

  user_op::TensorDesc* mut_tensor_meta() override { return impl_->mut_tensor_meta(); }

  Maybe<MirroredTensor> AsMirroredTensor() override { RETURN_ERROR_WITH_BUG_PROMPT(); }
  Maybe<ConsistentTensor> AsConsistentTensor() override { return shared_from_this(); }

 private:
  std::shared_ptr<ConsistentTensorImpl> impl_;
};

}  // namespace one
}  // namespace oneflow

#endif  // ONEFLOW_CORE_FRAMEWORK_TENSOR_H_
