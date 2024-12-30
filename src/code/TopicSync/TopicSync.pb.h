// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: TopicSync.proto

#ifndef GOOGLE_PROTOBUF_INCLUDED_TopicSync_2eproto
#define GOOGLE_PROTOBUF_INCLUDED_TopicSync_2eproto

#include <limits>
#include <string>

#include <google/protobuf/port_def.inc>
#if PROTOBUF_VERSION < 3021000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers. Please update
#error your headers.
#endif
#if 3021010 < PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers. Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/port_undef.inc>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata_lite.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>
#define PROTOBUF_INTERNAL_EXPORT_TopicSync_2eproto
PROTOBUF_NAMESPACE_OPEN
namespace internal {
class AnyMetadata;
}  // namespace internal
PROTOBUF_NAMESPACE_CLOSE

// Internal implementation detail -- do not use these members.
struct TableStruct_TopicSync_2eproto {
  static const uint32_t offsets[];
};
extern const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable descriptor_table_TopicSync_2eproto;
class TopicSync;
struct TopicSyncDefaultTypeInternal;
extern TopicSyncDefaultTypeInternal _TopicSync_default_instance_;
PROTOBUF_NAMESPACE_OPEN
template<> ::TopicSync* Arena::CreateMaybeMessage<::TopicSync>(Arena*);
PROTOBUF_NAMESPACE_CLOSE

// ===================================================================

class TopicSync final :
    public ::PROTOBUF_NAMESPACE_ID::Message /* @@protoc_insertion_point(class_definition:TopicSync) */ {
 public:
  inline TopicSync() : TopicSync(nullptr) {}
  ~TopicSync() override;
  explicit PROTOBUF_CONSTEXPR TopicSync(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized);

  TopicSync(const TopicSync& from);
  TopicSync(TopicSync&& from) noexcept
    : TopicSync() {
    *this = ::std::move(from);
  }

  inline TopicSync& operator=(const TopicSync& from) {
    CopyFrom(from);
    return *this;
  }
  inline TopicSync& operator=(TopicSync&& from) noexcept {
    if (this == &from) return *this;
    if (GetOwningArena() == from.GetOwningArena()
  #ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetOwningArena() != nullptr
  #endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::PROTOBUF_NAMESPACE_ID::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const TopicSync& default_instance() {
    return *internal_default_instance();
  }
  static inline const TopicSync* internal_default_instance() {
    return reinterpret_cast<const TopicSync*>(
               &_TopicSync_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    0;

  friend void swap(TopicSync& a, TopicSync& b) {
    a.Swap(&b);
  }
  inline void Swap(TopicSync* other) {
    if (other == this) return;
  #ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() != nullptr &&
        GetOwningArena() == other->GetOwningArena()) {
   #else  // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() == other->GetOwningArena()) {
  #endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::PROTOBUF_NAMESPACE_ID::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(TopicSync* other) {
    if (other == this) return;
    GOOGLE_DCHECK(GetOwningArena() == other->GetOwningArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  TopicSync* New(::PROTOBUF_NAMESPACE_ID::Arena* arena = nullptr) const final {
    return CreateMaybeMessage<TopicSync>(arena);
  }
  using ::PROTOBUF_NAMESPACE_ID::Message::CopyFrom;
  void CopyFrom(const TopicSync& from);
  using ::PROTOBUF_NAMESPACE_ID::Message::MergeFrom;
  void MergeFrom( const TopicSync& from) {
    TopicSync::MergeImpl(*this, from);
  }
  private:
  static void MergeImpl(::PROTOBUF_NAMESPACE_ID::Message& to_msg, const ::PROTOBUF_NAMESPACE_ID::Message& from_msg);
  public:
  PROTOBUF_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) final;
  uint8_t* _InternalSerialize(
      uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const final { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::PROTOBUF_NAMESPACE_ID::Arena* arena, bool is_message_owned);
  void SharedDtor();
  void SetCachedSize(int size) const final;
  void InternalSwap(TopicSync* other);

  private:
  friend class ::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata;
  static ::PROTOBUF_NAMESPACE_ID::StringPiece FullMessageName() {
    return "TopicSync";
  }
  protected:
  explicit TopicSync(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                       bool is_message_owned = false);
  public:

  static const ClassData _class_data_;
  const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*GetClassData() const final;

  ::PROTOBUF_NAMESPACE_ID::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kVTopicFieldNumber = 1,
  };
  // repeated string v_topic = 1;
  int v_topic_size() const;
  private:
  int _internal_v_topic_size() const;
  public:
  void clear_v_topic();
  const std::string& v_topic(int index) const;
  std::string* mutable_v_topic(int index);
  void set_v_topic(int index, const std::string& value);
  void set_v_topic(int index, std::string&& value);
  void set_v_topic(int index, const char* value);
  void set_v_topic(int index, const char* value, size_t size);
  std::string* add_v_topic();
  void add_v_topic(const std::string& value);
  void add_v_topic(std::string&& value);
  void add_v_topic(const char* value);
  void add_v_topic(const char* value, size_t size);
  const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField<std::string>& v_topic() const;
  ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField<std::string>* mutable_v_topic();
  private:
  const std::string& _internal_v_topic(int index) const;
  std::string* _internal_add_v_topic();
  public:

  // @@protoc_insertion_point(class_scope:TopicSync)
 private:
  class _Internal;

  template <typename T> friend class ::PROTOBUF_NAMESPACE_ID::Arena::InternalHelper;
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;
  struct Impl_ {
    ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField<std::string> v_topic_;
    mutable ::PROTOBUF_NAMESPACE_ID::internal::CachedSize _cached_size_;
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_TopicSync_2eproto;
};
// ===================================================================


// ===================================================================

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// TopicSync

// repeated string v_topic = 1;
inline int TopicSync::_internal_v_topic_size() const {
  return _impl_.v_topic_.size();
}
inline int TopicSync::v_topic_size() const {
  return _internal_v_topic_size();
}
inline void TopicSync::clear_v_topic() {
  _impl_.v_topic_.Clear();
}
inline std::string* TopicSync::add_v_topic() {
  std::string* _s = _internal_add_v_topic();
  // @@protoc_insertion_point(field_add_mutable:TopicSync.v_topic)
  return _s;
}
inline const std::string& TopicSync::_internal_v_topic(int index) const {
  return _impl_.v_topic_.Get(index);
}
inline const std::string& TopicSync::v_topic(int index) const {
  // @@protoc_insertion_point(field_get:TopicSync.v_topic)
  return _internal_v_topic(index);
}
inline std::string* TopicSync::mutable_v_topic(int index) {
  // @@protoc_insertion_point(field_mutable:TopicSync.v_topic)
  return _impl_.v_topic_.Mutable(index);
}
inline void TopicSync::set_v_topic(int index, const std::string& value) {
  _impl_.v_topic_.Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:TopicSync.v_topic)
}
inline void TopicSync::set_v_topic(int index, std::string&& value) {
  _impl_.v_topic_.Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:TopicSync.v_topic)
}
inline void TopicSync::set_v_topic(int index, const char* value) {
  GOOGLE_DCHECK(value != nullptr);
  _impl_.v_topic_.Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:TopicSync.v_topic)
}
inline void TopicSync::set_v_topic(int index, const char* value, size_t size) {
  _impl_.v_topic_.Mutable(index)->assign(
    reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:TopicSync.v_topic)
}
inline std::string* TopicSync::_internal_add_v_topic() {
  return _impl_.v_topic_.Add();
}
inline void TopicSync::add_v_topic(const std::string& value) {
  _impl_.v_topic_.Add()->assign(value);
  // @@protoc_insertion_point(field_add:TopicSync.v_topic)
}
inline void TopicSync::add_v_topic(std::string&& value) {
  _impl_.v_topic_.Add(std::move(value));
  // @@protoc_insertion_point(field_add:TopicSync.v_topic)
}
inline void TopicSync::add_v_topic(const char* value) {
  GOOGLE_DCHECK(value != nullptr);
  _impl_.v_topic_.Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:TopicSync.v_topic)
}
inline void TopicSync::add_v_topic(const char* value, size_t size) {
  _impl_.v_topic_.Add()->assign(reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:TopicSync.v_topic)
}
inline const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField<std::string>&
TopicSync::v_topic() const {
  // @@protoc_insertion_point(field_list:TopicSync.v_topic)
  return _impl_.v_topic_;
}
inline ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField<std::string>*
TopicSync::mutable_v_topic() {
  // @@protoc_insertion_point(field_mutable_list:TopicSync.v_topic)
  return &_impl_.v_topic_;
}

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include <google/protobuf/port_undef.inc>
#endif  // GOOGLE_PROTOBUF_INCLUDED_GOOGLE_PROTOBUF_INCLUDED_TopicSync_2eproto
