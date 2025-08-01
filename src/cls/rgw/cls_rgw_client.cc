// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <errno.h>

#include "cls/rgw/cls_rgw_const.h"
#include "cls/rgw/cls_rgw_client.h"

#include "common/debug.h"

using std::list;
using std::map;
using std::pair;
using std::string;
using std::vector;

using ceph::real_time;

using namespace librados;

const string BucketIndexShardsManager::KEY_VALUE_SEPARATOR = "#";
const string BucketIndexShardsManager::SHARDS_SEPARATOR = ",";

/**
 * This class represents the bucket index object operation callback context.
 */
template <typename T>
class ClsBucketIndexOpCtx : public ObjectOperationCompletion {
private:
  T *data;
  int *ret_code;
public:
  ClsBucketIndexOpCtx(T* _data, int *_ret_code) : data(_data), ret_code(_ret_code) { ceph_assert(data); }
  ~ClsBucketIndexOpCtx() override {}
  void handle_completion(int r, bufferlist& outbl) override {
    // if successful, or we're asked for a retry, copy result into
    // destination (*data)
    if (r >= 0 || r == RGWBIAdvanceAndRetryError) {
      try {
        auto iter = outbl.cbegin();
        decode((*data), iter);
      } catch (ceph::buffer::error& err) {
        r = -EIO;
      }
    }
    if (ret_code) {
      *ret_code = r;
    }
  }
};

void cls_rgw_bucket_init_index(ObjectWriteOperation& o)
{
  bufferlist in;
  o.exec(RGW_CLASS, RGW_BUCKET_INIT_INDEX, in);
}

void cls_rgw_bucket_init_index2(ObjectWriteOperation& o)
{
  bufferlist in;
  o.exec(RGW_CLASS, RGW_BUCKET_INIT_INDEX2, in);
}

void cls_rgw_bucket_set_tag_timeout(librados::ObjectWriteOperation& op,
                                    uint64_t timeout)
{
  const auto call = rgw_cls_tag_timeout_op{.tag_timeout = timeout};
  bufferlist in;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BUCKET_SET_TAG_TIMEOUT, in);
}

void cls_rgw_bucket_update_stats(librados::ObjectWriteOperation& o,
				 bool absolute,
                                 const map<RGWObjCategory, rgw_bucket_category_stats>& stats,
                                 const map<RGWObjCategory, rgw_bucket_category_stats>* dec_stats)
{
  rgw_cls_bucket_update_stats_op call;
  call.absolute = absolute;
  call.stats = stats;
  if (dec_stats != NULL)
    call.dec_stats = *dec_stats;
  bufferlist in;
  encode(call, in);
  o.exec(RGW_CLASS, RGW_BUCKET_UPDATE_STATS, in);
}

void cls_rgw_bucket_prepare_op(ObjectWriteOperation& o, RGWModifyOp op, const string& tag,
                               const cls_rgw_obj_key& key, const string& locator)
{
  rgw_cls_obj_prepare_op call;
  call.op = op;
  call.tag = tag;
  call.key = key;
  call.locator = locator;
  bufferlist in;
  encode(call, in);
  o.exec(RGW_CLASS, RGW_BUCKET_PREPARE_OP, in);
}

void cls_rgw_bucket_complete_op(ObjectWriteOperation& o, RGWModifyOp op, const string& tag,
                                const rgw_bucket_entry_ver& ver,
                                const cls_rgw_obj_key& key,
                                const rgw_bucket_dir_entry_meta& dir_meta,
				const list<cls_rgw_obj_key> *remove_objs, bool log_op,
                                uint16_t bilog_flags,
                                const rgw_zone_set *zones_trace,
				const std::string& obj_locator)
{

  bufferlist in;
  rgw_cls_obj_complete_op call;
  call.op = op;
  call.tag = tag;
  call.key = key;
  call.ver = ver;
  call.locator = obj_locator;
  call.meta = dir_meta;
  call.log_op = log_op;
  call.bilog_flags = bilog_flags;
  if (remove_objs)
    call.remove_objs = *remove_objs;
  if (zones_trace) {
    call.zones_trace = *zones_trace;
  }
  encode(call, in);
  o.exec(RGW_CLASS, RGW_BUCKET_COMPLETE_OP, in);
}

void cls_rgw_bucket_list_op(librados::ObjectReadOperation& op,
                            const cls_rgw_obj_key& start_obj,
                            const std::string& filter_prefix,
                            const std::string& delimiter,
                            uint32_t num_entries,
                            bool list_versions,
                            rgw_cls_list_ret* result)
{
  bufferlist in;
  rgw_cls_list_op call;
  call.start_obj = start_obj;
  call.filter_prefix = filter_prefix;
  call.delimiter = delimiter;
  call.num_entries = num_entries;
  call.list_versions = list_versions;
  encode(call, in);

  op.exec(RGW_CLASS, RGW_BUCKET_LIST, in,
	  new ClsBucketIndexOpCtx<rgw_cls_list_ret>(result, NULL));
}

void cls_rgw_remove_obj(librados::ObjectWriteOperation& o, list<string>& keep_attr_prefixes)
{
  bufferlist in;
  rgw_cls_obj_remove_op call;
  call.keep_attr_prefixes = keep_attr_prefixes;
  encode(call, in);
  o.exec(RGW_CLASS, RGW_OBJ_REMOVE, in);
}

void cls_rgw_obj_store_pg_ver(librados::ObjectWriteOperation& o, const string& attr)
{
  bufferlist in;
  rgw_cls_obj_store_pg_ver_op call;
  call.attr = attr;
  encode(call, in);
  o.exec(RGW_CLASS, RGW_OBJ_STORE_PG_VER, in);
}

void cls_rgw_obj_check_attrs_prefix(librados::ObjectOperation& o, const string& prefix, bool fail_if_exist)
{
  bufferlist in;
  rgw_cls_obj_check_attrs_prefix call;
  call.check_prefix = prefix;
  call.fail_if_exist = fail_if_exist;
  encode(call, in);
  o.exec(RGW_CLASS, RGW_OBJ_CHECK_ATTRS_PREFIX, in);
}

void cls_rgw_obj_check_mtime(librados::ObjectOperation& o, const real_time& mtime, bool high_precision_time, RGWCheckMTimeType type)
{
  bufferlist in;
  rgw_cls_obj_check_mtime call;
  call.mtime = mtime;
  call.high_precision_time = high_precision_time;
  call.type = type;
  encode(call, in);
  o.exec(RGW_CLASS, RGW_OBJ_CHECK_MTIME, in);
}

int cls_rgw_bi_get(librados::IoCtx& io_ctx, const string oid,
                   BIIndexType index_type, const cls_rgw_obj_key& key,
                   rgw_cls_bi_entry *entry)
{
  bufferlist in, out;
  rgw_cls_bi_get_op call;
  call.key = key;
  call.type = index_type;
  encode(call, in);
  int r = io_ctx.exec(oid, RGW_CLASS, RGW_BI_GET, in, out);
  if (r < 0)
    return r;

  rgw_cls_bi_get_ret op_ret;
  auto iter = out.cbegin();
  try {
    decode(op_ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }

  *entry = op_ret.entry;

  return 0;
}

int cls_rgw_bi_put(librados::IoCtx& io_ctx, const string oid, const rgw_cls_bi_entry& entry)
{
  bufferlist in, out;
  rgw_cls_bi_put_op call;
  call.entry = entry;
  encode(call, in);
  librados::ObjectWriteOperation op;
  op.exec(RGW_CLASS, RGW_BI_PUT, in);
  int r = io_ctx.operate(oid, &op);
  if (r < 0)
    return r;

  return 0;
}

void cls_rgw_bi_put(ObjectWriteOperation& op, const string oid, const rgw_cls_bi_entry& entry)
{
  bufferlist in, out;
  rgw_cls_bi_put_op call;
  call.entry = entry;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BI_PUT, in);
}

void cls_rgw_bi_put_entries(librados::ObjectWriteOperation& op,
                            std::vector<rgw_cls_bi_entry> entries,
                            bool check_existing)
{
  const auto call = rgw_cls_bi_put_entries_op{
    .entries = std::move(entries),
    .check_existing = check_existing
  };

  bufferlist in;
  encode(call, in);

  op.exec(RGW_CLASS, RGW_BI_PUT_ENTRIES, in);
}

/* nb: any entries passed in are replaced with the results of the cls
 * call, so caller does not need to clear entries between calls
 */
int cls_rgw_bi_list(librados::IoCtx& io_ctx, const std::string& oid,
		    const std::string& name_filter, const std::string& marker, uint32_t max,
		    std::list<rgw_cls_bi_entry> *entries, bool *is_truncated, bool reshardlog)
{
  bufferlist in, out;
  rgw_cls_bi_list_op call;
  call.name_filter = name_filter;
  call.marker = marker;
  call.max = max;
  call.reshardlog = reshardlog;
  encode(call, in);
  int r = io_ctx.exec(oid, RGW_CLASS, RGW_BI_LIST, in, out);
  if (r < 0)
    return r;

  rgw_cls_bi_list_ret op_ret;
  auto iter = out.cbegin();
  try {
    decode(op_ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }

  entries->swap(op_ret.entries);
  *is_truncated = op_ret.is_truncated;

  return 0;
}

int cls_rgw_bucket_link_olh(librados::IoCtx& io_ctx, const string& oid,
                            const cls_rgw_obj_key& key, const bufferlist& olh_tag,
                            bool delete_marker, const string& op_tag, const rgw_bucket_dir_entry_meta *meta,
                            uint64_t olh_epoch, ceph::real_time unmod_since, bool high_precision_time, bool log_op, const rgw_zone_set& zones_trace)
{
  librados::ObjectWriteOperation op;
  cls_rgw_bucket_link_olh(op, key, olh_tag, delete_marker, op_tag, meta,
                          olh_epoch, unmod_since, high_precision_time, log_op,
                          zones_trace);

  return io_ctx.operate(oid, &op);
}


void cls_rgw_bucket_link_olh(librados::ObjectWriteOperation& op, const cls_rgw_obj_key& key,
                            const bufferlist& olh_tag, bool delete_marker,
                            const string& op_tag, const rgw_bucket_dir_entry_meta *meta,
                            uint64_t olh_epoch, ceph::real_time unmod_since, bool high_precision_time, bool log_op, const rgw_zone_set& zones_trace)
{
  bufferlist in, out;
  rgw_cls_link_olh_op call;
  call.key = key;
  call.olh_tag = olh_tag.to_str();
  call.op_tag = op_tag;
  call.delete_marker = delete_marker;
  if (meta) {
    call.meta = *meta;
  }
  call.olh_epoch = olh_epoch;
  call.log_op = log_op;
  call.unmod_since = unmod_since;
  call.high_precision_time = high_precision_time;
  call.zones_trace = zones_trace;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BUCKET_LINK_OLH, in);
}

int cls_rgw_bucket_unlink_instance(librados::IoCtx& io_ctx, const string& oid,
                                   const cls_rgw_obj_key& key, const string& op_tag,
                                   const string& olh_tag, uint64_t olh_epoch, bool log_op,
                                   uint16_t bilog_flags, const rgw_zone_set& zones_trace)
{
  librados::ObjectWriteOperation op;
  cls_rgw_bucket_unlink_instance(op, key, op_tag, olh_tag, olh_epoch, log_op, bilog_flags, zones_trace);
  int r = io_ctx.operate(oid, &op);
  if (r < 0)
    return r;

  return 0;
}

void cls_rgw_bucket_unlink_instance(librados::ObjectWriteOperation& op,
                                   const cls_rgw_obj_key& key, const string& op_tag,
                                   const string& olh_tag, uint64_t olh_epoch, bool log_op,
                                   uint16_t bilog_flags, const rgw_zone_set& zones_trace)
{
  bufferlist in, out;
  rgw_cls_unlink_instance_op call;
  call.key = key;
  call.op_tag = op_tag;
  call.olh_epoch = olh_epoch;
  call.olh_tag = olh_tag;
  call.log_op = log_op;
  call.zones_trace = zones_trace;
  call.bilog_flags = bilog_flags;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BUCKET_UNLINK_INSTANCE, in);
}

void cls_rgw_get_olh_log(librados::ObjectReadOperation& op, const cls_rgw_obj_key& olh, uint64_t ver_marker, const string& olh_tag, rgw_cls_read_olh_log_ret& log_ret, int& op_ret)
{
  bufferlist in;
  rgw_cls_read_olh_log_op call;
  call.olh = olh;
  call.ver_marker = ver_marker;
  call.olh_tag = olh_tag;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BUCKET_READ_OLH_LOG, in, new ClsBucketIndexOpCtx<rgw_cls_read_olh_log_ret>(&log_ret, &op_ret));
}

int cls_rgw_get_olh_log(IoCtx& io_ctx, string& oid, const cls_rgw_obj_key& olh, uint64_t ver_marker,
                        const string& olh_tag,
                        rgw_cls_read_olh_log_ret& log_ret)
{
  int op_ret = 0;
  librados::ObjectReadOperation op;
  cls_rgw_get_olh_log(op, olh, ver_marker, olh_tag, log_ret, op_ret);
  int r = io_ctx.operate(oid, &op, NULL);
  if (r < 0) {
    return r;
  }
  if (op_ret < 0) {
    return op_ret;
  }

 return r;
}

void cls_rgw_trim_olh_log(librados::ObjectWriteOperation& op, const cls_rgw_obj_key& olh, uint64_t ver, const string& olh_tag)
{
  bufferlist in;
  rgw_cls_trim_olh_log_op call;
  call.olh = olh;
  call.ver = ver;
  call.olh_tag = olh_tag;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BUCKET_TRIM_OLH_LOG, in);
}

int cls_rgw_clear_olh(IoCtx& io_ctx, string& oid, const cls_rgw_obj_key& olh, const string& olh_tag)
{
  librados::ObjectWriteOperation op;
  cls_rgw_clear_olh(op, olh, olh_tag);

  return io_ctx.operate(oid, &op);
}

void cls_rgw_clear_olh(librados::ObjectWriteOperation& op, const cls_rgw_obj_key& olh, const string& olh_tag)
{
  bufferlist in;
  rgw_cls_bucket_clear_olh_op call;
  call.key = olh;
  call.olh_tag = olh_tag;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BUCKET_CLEAR_OLH, in);
}

void cls_rgw_bilog_list(librados::ObjectReadOperation& op,
                        const std::string& marker, uint32_t max,
                        cls_rgw_bi_log_list_ret *pdata, int *ret)
{
  cls_rgw_bi_log_list_op call;
  call.marker = marker;
  call.max = max;

  bufferlist in;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BI_LOG_LIST, in, new ClsBucketIndexOpCtx<cls_rgw_bi_log_list_ret>(pdata, ret));
}

void cls_rgw_bilog_trim(librados::ObjectWriteOperation& op,
                        const std::string& start_marker,
                        const std::string& end_marker)
{
  cls_rgw_bi_log_trim_op call;
  call.start_marker = start_marker;
  call.end_marker = end_marker;

  bufferlist in;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_BI_LOG_TRIM, in);
}

void cls_rgw_bucket_reshard_log_trim(librados::ObjectWriteOperation& op)
{
  bufferlist in;
  op.exec(RGW_CLASS, RGW_RESHARD_LOG_TRIM, in);
}

void cls_rgw_bucket_check_index(librados::ObjectReadOperation& op,
                                bufferlist& out)
{
  bufferlist in;
  op.exec(RGW_CLASS, RGW_BUCKET_CHECK_INDEX, in, &out, nullptr);
}

void cls_rgw_bucket_check_index_decode(const bufferlist& out,
                                       rgw_cls_check_index_ret& result)
{
  auto p = out.cbegin();
  decode(result, p);
}

void cls_rgw_bucket_rebuild_index(librados::ObjectWriteOperation& op)
{
  bufferlist in;
  op.exec(RGW_CLASS, RGW_BUCKET_REBUILD_INDEX, in);
}

void cls_rgw_encode_suggestion(char op, rgw_bucket_dir_entry& dirent, bufferlist& updates)
{
  updates.append(op);
  encode(dirent, updates);
}

void cls_rgw_suggest_changes(ObjectWriteOperation& o, bufferlist& updates)
{
  o.exec(RGW_CLASS, RGW_DIR_SUGGEST_CHANGES, updates);
}

void cls_rgw_bilog_start(ObjectWriteOperation& op)
{
  bufferlist in;
  op.exec(RGW_CLASS, RGW_BI_LOG_RESYNC, in);
}

void cls_rgw_bilog_stop(ObjectWriteOperation& op)
{
  bufferlist in;
  op.exec(RGW_CLASS, RGW_BI_LOG_STOP, in);
}

class GetDirHeaderCompletion : public ObjectOperationCompletion {
  boost::intrusive_ptr<RGWGetDirHeader_CB> cb;
public:
  explicit GetDirHeaderCompletion(boost::intrusive_ptr<RGWGetDirHeader_CB> cb)
    : cb(std::move(cb)) {}

  void handle_completion(int r, bufferlist& outbl) override {
    rgw_cls_list_ret ret;
    try {
      auto iter = outbl.cbegin();
      decode(ret, iter);
    } catch (ceph::buffer::error& err) {
      r = -EIO;
    }
    cb->handle_response(r, ret.dir.header);
  }
};

int cls_rgw_get_dir_header_async(IoCtx& io_ctx, const string& oid,
                                 boost::intrusive_ptr<RGWGetDirHeader_CB> cb)
{
  bufferlist in, out;
  rgw_cls_list_op call;
  call.num_entries = 0;
  encode(call, in);
  ObjectReadOperation op;
  op.exec(RGW_CLASS, RGW_BUCKET_LIST, in,
          new GetDirHeaderCompletion(std::move(cb)));
  AioCompletion *c = librados::Rados::aio_create_completion(nullptr, nullptr);
  int r = io_ctx.aio_operate(oid, c, &op, NULL);
  c->release();
  if (r < 0)
    return r;

  return 0;
}

int cls_rgw_usage_log_read(IoCtx& io_ctx, const string& oid, const string& user, const string& bucket,
                           uint64_t start_epoch, uint64_t end_epoch, uint32_t max_entries,
                           string& read_iter, map<rgw_user_bucket, rgw_usage_log_entry>& usage,
                           bool *is_truncated)
{
  if (is_truncated)
    *is_truncated = false;

  bufferlist in, out;
  rgw_cls_usage_log_read_op call;
  call.start_epoch = start_epoch;
  call.end_epoch = end_epoch;
  call.owner = user;
  call.max_entries = max_entries;
  call.bucket = bucket;
  call.iter = read_iter;
  encode(call, in);
  int r = io_ctx.exec(oid, RGW_CLASS, RGW_USER_USAGE_LOG_READ, in, out);
  if (r < 0)
    return r;

  try {
    rgw_cls_usage_log_read_ret result;
    auto iter = out.cbegin();
    decode(result, iter);
    read_iter = result.next_iter;
    if (is_truncated)
      *is_truncated = result.truncated;

    usage = result.usage;
  } catch (ceph::buffer::error& e) {
    return -EINVAL;
  }

  return 0;
}

int cls_rgw_usage_log_trim(IoCtx& io_ctx, const string& oid, const string& user, const string& bucket,
			   uint64_t start_epoch, uint64_t end_epoch)
{
  bufferlist in;
  rgw_cls_usage_log_trim_op call;
  call.start_epoch = start_epoch;
  call.end_epoch = end_epoch;
  call.user = user;
  call.bucket = bucket;
  encode(call, in);

  bool done = false;
  do {
    ObjectWriteOperation op;
    op.exec(RGW_CLASS, RGW_USER_USAGE_LOG_TRIM, in);
    int r = io_ctx.operate(oid, &op);
    if (r == -ENODATA)
      done = true;
    else if (r < 0)
      return r;
  } while (!done);

  return 0;
}

void cls_rgw_usage_log_trim(librados::ObjectWriteOperation& op, const string& user, const string& bucket, uint64_t start_epoch, uint64_t end_epoch)
{
  bufferlist in;
  rgw_cls_usage_log_trim_op call;
  call.start_epoch = start_epoch;
  call.end_epoch = end_epoch;
  call.user = user;
  call.bucket = bucket;
  encode(call, in);

  op.exec(RGW_CLASS, RGW_USER_USAGE_LOG_TRIM, in);
}

void cls_rgw_usage_log_clear(ObjectWriteOperation& op)
{
  bufferlist in;
  op.exec(RGW_CLASS, RGW_USAGE_LOG_CLEAR, in);
}

void cls_rgw_usage_log_add(ObjectWriteOperation& op, rgw_usage_log_info& info)
{
  bufferlist in;
  rgw_cls_usage_log_add_op call;
  call.info = info;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_USER_USAGE_LOG_ADD, in);
}

/* garbage collection */

void cls_rgw_gc_set_entry(ObjectWriteOperation& op, uint32_t expiration_secs, cls_rgw_gc_obj_info& info)
{
  bufferlist in;
  cls_rgw_gc_set_entry_op call;
  call.expiration_secs = expiration_secs;
  call.info = info;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_GC_SET_ENTRY, in);
}

void cls_rgw_gc_defer_entry(ObjectWriteOperation& op, uint32_t expiration_secs, const string& tag)
{
  bufferlist in;
  cls_rgw_gc_defer_entry_op call;
  call.expiration_secs = expiration_secs;
  call.tag = tag;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_GC_DEFER_ENTRY, in);
}

void cls_rgw_gc_list(ObjectReadOperation& op, const string& marker,
                     uint32_t max, bool expired_only, bufferlist& out)
{
  bufferlist in;
  cls_rgw_gc_list_op call;
  call.marker = marker;
  call.max = max;
  call.expired_only = expired_only;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_GC_LIST, in, &out, nullptr);
}

int cls_rgw_gc_list_decode(const bufferlist& out,
                           std::list<cls_rgw_gc_obj_info>& entries,
                           bool *truncated, std::string& next_marker)
{
  cls_rgw_gc_list_ret ret;
  try {
    auto iter = out.cbegin();
    decode(ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }

  entries.swap(ret.entries);

  if (truncated)
    *truncated = ret.truncated;
  next_marker = std::move(ret.next_marker);
  return 0;
}

void cls_rgw_gc_remove(librados::ObjectWriteOperation& op, const vector<string>& tags)
{
  bufferlist in;
  cls_rgw_gc_remove_op call;
  call.tags = tags;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_GC_REMOVE, in);
}

void cls_rgw_lc_get_head(ObjectReadOperation& op, bufferlist& out)
{
  bufferlist in;
  op.exec(RGW_CLASS, RGW_LC_GET_HEAD, in, &out, nullptr);
}

int cls_rgw_lc_get_head_decode(const bufferlist& out, cls_rgw_lc_obj_head& head)
{
  cls_rgw_lc_get_head_ret ret;
  try {
    auto iter = out.cbegin();
    decode(ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }
  head = std::move(ret.head);

  return 0;
}

void cls_rgw_lc_put_head(ObjectWriteOperation& op, const cls_rgw_lc_obj_head& head)
{
  bufferlist in;
  cls_rgw_lc_put_head_op call;
  call.head = head;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_LC_PUT_HEAD, in);
}

void cls_rgw_lc_get_next_entry(ObjectReadOperation& op, const string& marker,
                               bufferlist& out)
{
  bufferlist in;
  cls_rgw_lc_get_next_entry_op call;
  call.marker = marker;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_LC_GET_NEXT_ENTRY, in, &out, nullptr);
}

int cls_rgw_lc_get_next_entry_decode(const bufferlist& out, cls_rgw_lc_entry& entry)
{
  cls_rgw_lc_get_next_entry_ret ret;
  try {
    auto iter = out.cbegin();
    decode(ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }
  entry = std::move(ret.entry);

  return 0;
}

void cls_rgw_lc_rm_entry(ObjectWriteOperation& op,
                         const cls_rgw_lc_entry& entry)
{
  bufferlist in;
  cls_rgw_lc_rm_entry_op call;
  call.entry = entry;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_LC_RM_ENTRY, in);
}

void cls_rgw_lc_set_entry(ObjectWriteOperation& op,
                          const cls_rgw_lc_entry& entry)
{
  bufferlist in, out;
  cls_rgw_lc_set_entry_op call;
  call.entry = entry;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_LC_SET_ENTRY, in);
}

void cls_rgw_lc_get_entry(ObjectReadOperation& op, const std::string& marker,
                          bufferlist& out)
{
  bufferlist in;
  cls_rgw_lc_get_entry_op call{marker};
  encode(call, in);
  op.exec(RGW_CLASS, RGW_LC_GET_ENTRY, in, &out, nullptr);
}

int cls_rgw_lc_get_entry_decode(const bufferlist& out, cls_rgw_lc_entry& entry)
{
  cls_rgw_lc_get_entry_ret ret;
  try {
    auto iter = out.cbegin();
    decode(ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }

  entry = std::move(ret.entry);
  return 0;
}

void cls_rgw_lc_list(ObjectReadOperation& op, const string& marker,
                     uint32_t max_entries, bufferlist& out)
{
  bufferlist in;
  cls_rgw_lc_list_entries_op call;
  call.marker = marker;
  call.max_entries = max_entries;

  encode(call, in);

  op.exec(RGW_CLASS, RGW_LC_LIST_ENTRIES, in, &out, nullptr);
}

int cls_rgw_lc_list_decode(const bufferlist& out, std::vector<cls_rgw_lc_entry>& entries)
{
  cls_rgw_lc_list_entries_ret ret;
  try {
    auto iter = out.cbegin();
    decode(ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }

  std::sort(std::begin(ret.entries), std::end(ret.entries),
	    [](const cls_rgw_lc_entry& a, const cls_rgw_lc_entry& b)
	      { return a.bucket < b.bucket; });
  entries = std::move(ret.entries);
  return 0;
}

void cls_rgw_mp_upload_part_info_update(librados::ObjectWriteOperation& op,
                                        const std::string& part_key,
                                        const RGWUploadPartInfo& info)
{
  cls_rgw_mp_upload_part_info_update_op call;
  call.part_key = part_key;
  call.info     = info;

  buffer::list in;
  encode(call, in);

  op.exec(RGW_CLASS, RGW_MP_UPLOAD_PART_INFO_UPDATE, in);
}

void cls_rgw_reshard_add(librados::ObjectWriteOperation& op,
			 const cls_rgw_reshard_entry& entry,
			 const bool create_only)
{
  bufferlist in;
  cls_rgw_reshard_add_op call;
  call.entry = entry;
  call.create_only = create_only;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_RESHARD_ADD, in);
}

int cls_rgw_reshard_list(librados::IoCtx& io_ctx, const string& oid, string& marker, uint32_t max,
                         list<cls_rgw_reshard_entry>& entries, bool* is_truncated)
{
  bufferlist in, out;
  cls_rgw_reshard_list_op call;
  call.marker = marker;
  call.max = max;
  encode(call, in);
  int r = io_ctx.exec(oid, RGW_CLASS, RGW_RESHARD_LIST, in, out);
  if (r < 0)
    return r;

  cls_rgw_reshard_list_ret op_ret;
  auto iter = out.cbegin();
  try {
    decode(op_ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }

  entries.swap(op_ret.entries);
  *is_truncated = op_ret.is_truncated;

  return 0;
}

int cls_rgw_reshard_get(librados::IoCtx& io_ctx, const string& oid, cls_rgw_reshard_entry& entry)
{
  bufferlist in, out;
  cls_rgw_reshard_get_op call;
  call.entry = entry;
  encode(call, in);
  int r = io_ctx.exec(oid, RGW_CLASS, RGW_RESHARD_GET, in, out);
  if (r < 0)
    return r;

  cls_rgw_reshard_get_ret op_ret;
  auto iter = out.cbegin();
  try {
    decode(op_ret, iter);
  } catch (ceph::buffer::error& err) {
    return -EIO;
  }

  entry = op_ret.entry;

  return 0;
}

void cls_rgw_reshard_remove(librados::ObjectWriteOperation& op, const cls_rgw_reshard_entry& entry)
{
  bufferlist in;
  cls_rgw_reshard_remove_op call;
  call.tenant = entry.tenant;
  call.bucket_name = entry.bucket_name;
  call.bucket_id = entry.bucket_id;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_RESHARD_REMOVE, in);
}

void cls_rgw_clear_bucket_resharding(librados::ObjectWriteOperation& op)
{
  bufferlist in;
  cls_rgw_clear_bucket_resharding_op call;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_CLEAR_BUCKET_RESHARDING, in);
}

void cls_rgw_get_bucket_resharding(librados::ObjectReadOperation& op,
                                   bufferlist& out)
{
  bufferlist in;
  cls_rgw_get_bucket_resharding_op call;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_GET_BUCKET_RESHARDING, in, &out, nullptr);
}

void cls_rgw_get_bucket_resharding_decode(const bufferlist& out,
                                          cls_rgw_bucket_instance_entry& entry)
{
  cls_rgw_get_bucket_resharding_ret op_ret;
  auto iter = out.cbegin();
  decode(op_ret, iter);

  entry = std::move(op_ret.new_instance);
}

void cls_rgw_guard_bucket_resharding(librados::ObjectOperation& op, int ret_err)
{
  bufferlist in, out;
  cls_rgw_guard_bucket_resharding_op call;
  call.ret_err = ret_err;
  encode(call, in);
  op.exec(RGW_CLASS, RGW_GUARD_BUCKET_RESHARDING, in);
}

void cls_rgw_set_bucket_resharding(librados::ObjectWriteOperation& op,
                                   cls_rgw_reshard_status status)
{
  bufferlist in;
  cls_rgw_set_bucket_resharding_op call;
  call.entry.reshard_status = status;
  encode(call, in);

  op.assert_exists(); // the shard must exist; if not fail rather than recreate
  op.exec(RGW_CLASS, RGW_SET_BUCKET_RESHARDING, in);
}
