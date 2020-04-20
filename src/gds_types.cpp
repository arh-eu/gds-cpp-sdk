#include "gds_types.hpp"

#include <iostream>

namespace gds_lib {
namespace gds_types {

void GdsMessage::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(11);
  packer.pack(userName);
  packer.pack(messageId);
  packer.pack_int64(createTime);
  packer.pack_int64(requestTime);
  if (isFragmented) {
    packer.pack_true();
    packer.pack(firstFragment.value());
    packer.pack(lastFragment.value());
    packer.pack(offset.value());
    packer.pack(fds.value());
  } else {
    packer.pack_false();
    packer.pack_nil(); // first fragmented
    packer.pack_nil(); // last fragmented
    packer.pack_nil(); // offset
    packer.pack_nil(); // full data size
  }
  packer.pack_int32(dataType); // datatype
  if (messageBody) {
    messageBody->pack(packer);
  } else {
    packer.pack_nil();
  }
}

void GdsMessage::unpack(const msgpack::object &object) {
  std::vector<msgpack::object> data = object.as<std::vector<msgpack::object>>();
  userName = data.at(gds_types::GdsHeader::USER).as<std::string>();
  messageId = data.at(gds_types::GdsHeader::ID).as<std::string>();
  createTime = data.at(gds_types::GdsHeader::CREATE_TIME).as<int64_t>();
  requestTime = data.at(gds_types::GdsHeader::REQUEST_TIME).as<int64_t>();
  isFragmented = data.at(gds_types::GdsHeader::FRAGMENTED).as<bool>();
  if (isFragmented) {
    firstFragment =
        data.at(gds_types::GdsHeader::FIRST_FRAGMENT).as<std::string>();
    lastFragment =
        data.at(gds_types::GdsHeader::LAST_FRAGMENT).as<std::string>();
    offset = data.at(gds_types::GdsHeader::OFFSET).as<int32_t>();
    fds = data.at(gds_types::GdsHeader::FULL_DATA_SIZE).as<int32_t>();
  } else {
    firstFragment.reset();
    lastFragment.reset();
    offset.reset();
    fds.reset();
  }
  dataType = data.at(gds_types::GdsHeader::DATA_TYPE).as<int32_t>();

  switch (dataType) {
  case gds_types::GdsMsgType::LOGIN: // Type 0
    messageBody.reset(new GdsLoginMessage());
    break;
  case gds_types::GdsMsgType::LOGIN_REPLY: // Type 1
    messageBody.reset(new GdsLoginReplyMessage());
    break;
  case gds_types::GdsMsgType::EVENT: // Type 2
    messageBody.reset(new GdsEventMessage());
    break;
  case gds_types::GdsMsgType::EVENT_REPLY: // Type 3
    messageBody.reset(new GdsEventReplyMessage());
    break;
  case gds_types::GdsMsgType::ATTACHMENT_REQUEST: // Type 4
    messageBody.reset(new GdsAttachmentRequestMessage());
    break;
  case gds_types::GdsMsgType::ATTACHMENT_REQUEST_REPLY: // Type 5
    messageBody.reset(new GdsAttachmentRequestReplyMessage());
    break;
  case gds_types::GdsMsgType::ATTACHMENT: // Type 6
    messageBody.reset(new GdsAttachmentResponseMessage());
    break;
  case gds_types::GdsMsgType::ATTACHMENT_REPLY: // Type 7
    messageBody.reset(new GdsAttachmentResponseResultMessage());
    break;
  case gds_types::GdsMsgType::EVENT_DOCUMENT: // Type 8
    messageBody.reset(new GdsEventDocumentMessage());
    break;
  case gds_types::GdsMsgType::EVENT_DOCUMENT_REPLY: // Type 9
    messageBody.reset(new GdsEventDocumentReplyMessage());
    break;
  case gds_types::GdsMsgType::QUERY: // Type 10
    messageBody.reset(new GdsQueryRequestMessage());
    break;
  case gds_types::GdsMsgType::QUERY_REPLY: // Type 11
    messageBody.reset(new GdsQueryReplyMessage());
    break;
  case gds_types::GdsMsgType::GET_NEXT_QUERY: // Type 12
    messageBody.reset(new GdsNextQueryRequestMessage());
    break;
  default:
    messageBody.reset();
    break;
  }
  if (messageBody) {
    messageBody->unpack(data.at(gds_types::GdsHeader::DATA));
  }

  validate();
}

void GdsMessage::validate() const {
  if (createTime < 0) {
    throw invalid_message_error(GdsMsgType::HEADER_MESSAGE);
  }
  if (isFragmented) {
    if (!(firstFragment.has_value() && lastFragment.has_value() &&
          offset.has_value() && fds.has_value())) {
      throw invalid_message_error(GdsMsgType::HEADER_MESSAGE);
    }
  } else {
    if ((firstFragment.has_value() || lastFragment.has_value() ||
         offset.has_value() || fds.has_value())) {
      throw invalid_message_error(GdsMsgType::HEADER_MESSAGE);
    }
  }
  if (dataType < 0 || dataType > 14) {
    throw invalid_message_error(GdsMsgType::HEADER_MESSAGE);
  }
  if (messageBody) {
    messageBody->validate();
  }
}

void GdsFieldValue::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  switch (type) {
  case msgpack::type::NIL:
    packer.pack_nil();
    break;
  case msgpack::type::BOOLEAN:
    as<bool>() ? packer.pack_true() : packer.pack_false();
    break;
  case msgpack::type::POSITIVE_INTEGER:
    packer.pack_uint64(as<uint64_t>());
    break;
  case msgpack::type::NEGATIVE_INTEGER:
    packer.pack_int64(as<int64_t>());
    break;
  case msgpack::type::FLOAT32:
    packer.pack_float(as<float>());
    break;
  case msgpack::type::FLOAT64:
    packer.pack_double(as<double>());
    break;
  case msgpack::type::STR:
    packer.pack(as<std::string>());
    break;
  case msgpack::type::BIN:
    packer.pack(as<byte_array>());
    break;
  case msgpack::type::ARRAY: {
    std::vector<GdsFieldValue> items = as<std::vector<GdsFieldValue>>();
    packer.pack_array(items.size());
    for (auto &obj : items) {
      obj.pack(packer);
    }
  } break;
  case msgpack::type::MAP:
    packer.pack(as<std::map<std::string, std::string>>());
    break;
  default:
    throw invalid_message_error(GdsMsgType::UNKNOWN);
  }
}

void GdsFieldValue::unpack(const msgpack::object &obj) {
  type = obj.type;
  switch (obj.type) {
  case msgpack::type::NIL:
    value = std::nullopt;
    break;
  case msgpack::type::BOOLEAN:
    value = obj.as<bool>();
    break;
  case msgpack::type::POSITIVE_INTEGER:
    value = obj.as<uint64_t>();
    break;
  case msgpack::type::NEGATIVE_INTEGER:
    value = obj.as<int64_t>();
    break;
  case msgpack::type::FLOAT32:
    value = obj.as<float>();
    break;
  case msgpack::type::FLOAT64:
    value = obj.as<double>();
    break;
  case msgpack::type::STR:
    value = obj.as<std::string>();
    break;
  case msgpack::type::BIN:
    value = obj.as<byte_array>();
    break;
  case msgpack::type::ARRAY: {
    std::vector<msgpack::object> items = obj.as<std::vector<msgpack::object>>();
    std::vector<GdsFieldValue>   values;
    values.reserve(items.size());
    for (auto &object : items) {
      GdsFieldValue current;
      current.unpack(object);
      values.emplace_back(current);
    }
    value = values;
  } break;
  case msgpack::type::MAP:
    value = obj.as<std::map<std::string, std::string>>();
    break;
  default:
    throw invalid_message_error(GdsMsgType::UNKNOWN);
  }
}

void GdsFieldValue::validate() const {}

void EventReplyBody::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  for (auto &eventResult : results) {
    packer.pack_array(4);

    packer.pack_int32(eventResult.status);
    packer.pack(eventResult.notification);

    packer.pack_array(eventResult.fieldDescriptor.size());
    for (auto &descriptor : eventResult.fieldDescriptor) {
      packer.pack_array(descriptor.size());
      for (auto &item : descriptor) {
        packer.pack(item);
      }
    }

    packer.pack_array(eventResult.subResults.size());
    for (auto &subResult : eventResult.subResults) {
      packer.pack_array(6);
      packer.pack_int32(subResult.status);

      if (subResult.id.has_value()) {
        packer.pack(subResult.id.value());
      } else {
        packer.pack_nil();
      }
      if (subResult.tableName.has_value()) {
        packer.pack(subResult.tableName.value());
      } else {
        packer.pack_nil();
      }
      if (subResult.created.has_value()) {
        subResult.created.value() ? packer.pack_true() : packer.pack_false();
      } else {
        packer.pack_nil();
      }
      if (subResult.version.has_value()) {
        packer.pack_int64(subResult.version.value());
      } else {
        packer.pack_nil();
      }
      if (subResult.values.has_value()) {
        packer.pack_array(subResult.values.value().size());
        for (auto &fieldValue : subResult.values.value()) {
          fieldValue.pack(packer);
        }
      } else {
        packer.pack_nil();
      }
    }
  }
}

void EventReplyBody::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> eventResults =
      packer.as<std::vector<msgpack::object>>();

  results.reserve(eventResults.size());

  for (auto &object : eventResults) {
    std::vector<msgpack::object> currentData =
        object.as<std::vector<msgpack::object>>();

    GdsEventResult currentResult;

    currentResult.status = currentData.at(0).as<int32_t>();
    if (!currentData.at(0).is_nil()) {
      currentResult.notification = currentData.at(1).as<std::string>();
    }
    currentResult.fieldDescriptor =
        currentData.at(2).as<std::vector<field_descriptor>>();

    std::vector<msgpack::object> subResults =
        currentData.at(3).as<std::vector<msgpack::object>>();
    currentResult.subResults.reserve(subResults.size());

    for (auto &subResultObj : subResults) {
      std::vector<msgpack::object> subRes =
          subResultObj.as<std::vector<msgpack::object>>();
      EventSubResult currentSubResult;

      currentSubResult.status = subRes.at(0).as<int32_t>();

      if (!subRes.at(1).is_nil()) {
        currentSubResult.id = subRes.at(1).as<std::string>();
      }

      if (!subRes.at(2).is_nil()) {
        currentSubResult.tableName = subRes.at(2).as<std::string>();
      }

      if (!subRes.at(3).is_nil()) {
        currentSubResult.created = subRes.at(3).as<bool>();
      }

      if (!subRes.at(4).is_nil()) {
        currentSubResult.version = subRes.at(4).as<int64_t>();
      }

      std::vector<msgpack::object> fieldValues =
          subRes.at(5).as<std::vector<msgpack::object>>();

      currentSubResult.values = std::vector<GdsFieldValue>();
      currentSubResult.values.value().reserve(fieldValues.size());
      for (auto &fieldValue : fieldValues) {
        GdsFieldValue currentGDSFv;
        currentGDSFv.unpack(fieldValue);
        currentSubResult.values.value().emplace_back(currentGDSFv);
      }

      currentResult.subResults.emplace_back(currentSubResult);
    }

    results.emplace_back(currentResult);
  }

  validate();
}

void EventReplyBody::validate() const {
  // skip
}

void AttachmentResult::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  size_t map_elements = 4;
  if (meta.has_value()) {
    map_elements += 1;
  }
  if (ttl.has_value()) {
    map_elements += 1;
  }
  if (to_valid.has_value()) {
    map_elements += 1;
  }
  if (attachment.has_value()) {
    map_elements += 1;
  }

  packer.pack_map(map_elements);
  packer.pack("requestids");
  packer.pack(requestIDs);
  packer.pack("ownertable");
  packer.pack(ownerTable);
  packer.pack("attachmentid");
  packer.pack(attachmentID);
  packer.pack("ownerids");
  packer.pack(ownerIDs);

  if (meta.has_value()) {
    packer.pack("meta");
    packer.pack(meta.value());
  }
  if (ttl.has_value()) {
    packer.pack("ttl");
    packer.pack_int64(ttl.value());
  }
  if (to_valid.has_value()) {
    packer.pack("to_valid");
    packer.pack_int64(to_valid.value());
  }
  if (attachment.has_value()) {
    packer.pack("attachment");
    packer.pack(attachment.value());
  }
}

void AttachmentResult::unpack(const msgpack::object &packer) {
  std::map<std::string, msgpack::object> obj =
      packer.as<std::map<std::string, msgpack::object>>();

  requestIDs = obj.at("requestids").as<std::vector<std::string>>();
  ownerTable = obj.at("ownertable").as<std::string>();
  attachmentID = obj.at("attachmentid").as<std::string>();
  ownerIDs = obj.at("ownerids").as<std::vector<std::string>>();

  if (obj.find("meta") != obj.end()) {
    meta = obj.at("meta").as<std::string>();
  }
  if (obj.find("ttl") != obj.end()) {
    ttl = obj.at("ttl").as<int64_t>();
  }
  if (obj.find("to_valid") != obj.end()) {
    to_valid = obj.at("to_valid").as<int64_t>();
  }
  if (obj.find("attachment") != obj.end()) {
    attachment = obj.at("attachment").as<byte_array>();
  }
  validate();
}
void AttachmentResult::validate() const {}

void AttachmentRequestBody::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(3);
  packer.pack_int32(status);
  result.pack(packer);
  if (waitTime.has_value()) {
    packer.pack_int64(waitTime.value());
  } else {
    packer.pack_nil();
  }
}

void AttachmentRequestBody::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> obj = packer.as<std::vector<msgpack::object>>();
  status = obj.at(0).as<int32_t>();
  result.unpack(obj.at(1));
  if (obj.at(2).is_nil()) {
    waitTime.reset();
  } else {
    waitTime = obj.at(2).as<int64_t>();
  }
  validate();
}
void AttachmentRequestBody::validate() const {}

void AttachmentResponse::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_map(3);
  packer.pack("requestids");
  packer.pack(requestIDs);
  packer.pack("ownertable");
  packer.pack(ownerTable);
  packer.pack("attachmentid");
  packer.pack(attachmentID);
}

void AttachmentResponse::unpack(const msgpack::object &packer) {
  std::map<std::string, msgpack::object> obj =
      packer.as<std::map<std::string, msgpack::object>>();

  requestIDs = obj.at("requestids").as<std::vector<std::string>>();
  ownerTable = obj.at("ownertable").as<std::string>();
  attachmentID = obj.at("attachmentid").as<std::string>();
  validate();
}
void AttachmentResponse::validate() const {}

void AttachmentResponseBody::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(2);
  packer.pack_int32(status);
  result.pack(packer);
}

void AttachmentResponseBody::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> obj = packer.as<std::vector<msgpack::object>>();
  status = obj.at(0).as<int32_t>();
  result.unpack(obj.at(1));
  validate();
}
void AttachmentResponseBody::validate() const { result.validate(); }

void EventDocumentResult::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(3);
  packer.pack_int32(status_code);
  if (notification.has_value()) {
    packer.pack(notification.value());
  } else {
    packer.pack_nil();
  }
  packer.pack_map(0);
}

void EventDocumentResult::unpack(const msgpack::object &object) {
  std::vector<msgpack::object> data = object.as<std::vector<msgpack::object>>();
  status_code = data.at(0).as<int32_t>();
  if (!data.at(1).is_nil()) {
    notification = data.at(1).as<std::string>();
  }
  validate();
}
void EventDocumentResult::validate() const {}

void QueryContextDescriptor::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  packer.pack_array(9);

  packer.pack(scroll_id);
  packer.pack(select_query);
  packer.pack_int64(delivered_hits);
  packer.pack_int64(query_start_time);
  packer.pack(consistency_type);
  packer.pack(last_bucket_id);
  packer.pack_array(2);
  packer.pack(gds_holder.at(0));
  packer.pack(gds_holder.at(1));

  packer.pack_array(field_values.size());
  for (auto &item : field_values) {
    item.pack(packer);
  }

  packer.pack_array(partition_names.size());
  for (auto &item : partition_names) {
    packer.pack(item);
  }
}
void QueryContextDescriptor::unpack(const msgpack::object &object) {
  std::vector<msgpack::object> data = object.as<std::vector<msgpack::object>>();
  scroll_id = data.at(0).as<std::string>();
  select_query = data.at(1).as<std::string>();
  delivered_hits = data.at(2).as<int64_t>();
  query_start_time = data.at(3).as<int64_t>();
  consistency_type = data.at(4).as<std::string>();
  last_bucket_id = data.at(5).as<std::string>();

  std::vector<std::string> gdsholders =
      data.at(6).as<std::vector<std::string>>();
  gds_holder[0] = gdsholders.at(0);
  gds_holder[1] = gdsholders.at(1);

  std::vector<msgpack::object> fieldvalues =
      data.at(7).as<std::vector<msgpack::object>>();
  fieldvalues.clear();
  field_values.reserve(fieldvalues.size());
  for (auto &val : fieldvalues) {
    GdsFieldValue current;
    current.unpack(val);
    field_values.emplace_back(current);
  }
  partition_names = data.at(8).as<std::vector<std::string>>();
}

void QueryContextDescriptor::validate() const {}

void QueryReplyBody::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(6);
  packer.pack_int64(numberOfHits);
  packer.pack_int64(filteredHits);
  hasMorePages ? packer.pack_true() : packer.pack_false();
  queryContextDescriptor.pack(packer);
  packer.pack_array(fieldDescriptors.size());
  for (auto &item : fieldDescriptors) {
    packer.pack_array(3);
    for (auto &desc : item) {
      packer.pack(desc);
    }
  }

  packer.pack_array(hits.size());
  for (auto &hit_rows : hits) {
    packer.pack_array(hit_rows.size());
    for (auto &hit : hit_rows) {
      hit.pack(packer);
    }
  }
}

void QueryReplyBody::unpack(const msgpack::object &obj) {
  std::vector<msgpack::object> items = obj.as<std::vector<msgpack::object>>();
  numberOfHits = items.at(0).as<int64_t>();
  filteredHits = items.at(1).as<int64_t>();
  hasMorePages = items.at(2).as<bool>();
  queryContextDescriptor.unpack(items.at(3));
  std::vector<msgpack::object> fielddescriptors =
      items.at(4).as<std::vector<msgpack::object>>();
  fieldDescriptors.reserve(fielddescriptors.size());
  for (auto &item : fielddescriptors) {
    std::vector<std::string> data = item.as<std::vector<std::string>>();
    field_descriptor         desc;
    desc[0] = data.at(0);
    desc[1] = data.at(1);
    desc[2] = data.at(2);
    fieldDescriptors.emplace_back(desc);
  }

  std::vector<msgpack::object> values =
      items.at(5).as<std::vector<msgpack::object>>();
  hits.reserve(values.size());
  for (auto &object : values) {
    std::vector<msgpack::object> row =
        object.as<std::vector<msgpack::object>>();
    std::vector<GdsFieldValue> currenthit;
    currenthit.reserve(row.size());
    for (auto &val : row) {
      GdsFieldValue value;
      value.unpack(val);
      currenthit.emplace_back(value);
    }

    hits.emplace_back(currenthit);
  }

  validate();
}
void QueryReplyBody::validate() const {
  if (hits.size() != numberOfHits) {
    throw invalid_message_error(GdsMsgType::QUERY_REPLY);
  }
}

/*0*/
void GdsLoginMessage::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();

  packer.pack_array(reserved_fields.has_value() ? 5 : 4);
  serve_on_the_same_connection ? packer.pack_true() : packer.pack_false();
  packer.pack_int32(protocol_version_number);
  if (fragmentation_supported) {
    packer.pack_true();
    packer.pack_int32(fragment_transmission_unit.value());
  } else {
    packer.pack_false();
    packer.pack_nil();
  }

  if (reserved_fields.has_value()) {
    packer.pack_array(reserved_fields.value().size());
    for (auto &field : reserved_fields.value()) {
      packer.pack(field);
    }
  }
}

void GdsLoginMessage::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();

  serve_on_the_same_connection = data.at(0).as<bool>();
  protocol_version_number = data.at(1).as<int32_t>();
  fragmentation_supported = data.at(2).as<bool>();
  if (fragmentation_supported) {
    fragment_transmission_unit = data.at(3).as<int32_t>();
  } else {
    fragment_transmission_unit.reset();
  }

  if (data.size() == 5) {
    reserved_fields = data.at(4).as<std::vector<std::string>>();
  } else {
    reserved_fields.reset();
  }

  validate();
}

void GdsLoginMessage::validate() const {
  if (fragmentation_supported != fragment_transmission_unit.has_value()) {
    throw invalid_message_error(GdsMsgType::LOGIN, "fragmentation_flag");
  }

  if (reserved_fields.has_value() && reserved_fields.value().size() == 0) {
    throw invalid_message_error(GdsMsgType::LOGIN, "reserved_fields");
  }
}

/*1*/
void GdsLoginReplyMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(3);
  packer.pack_int32(ackStatus);
  if (loginReply) {
    loginReply.value().pack(packer);
  } else if (errorDetails) {
    packer.pack_map(errorDetails.value().size());
    for (auto &pair : errorDetails.value()) {
      packer.pack_int32(pair.first);
      packer.pack(pair.second);
    }
  } else {
    packer.pack_nil();
  }
  if (ackException) {
    packer.pack(ackException);
  } else {
    packer.pack_nil();
  }
}

void GdsLoginReplyMessage::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();
  ackStatus = data.at(0).as<int32_t>();
  loginReply.reset();
  errorDetails.reset();

  if (data.at(1).type == msgpack::type::ARRAY) {
    GdsLoginMessage loginmessage;
    loginmessage.unpack(data.at(1));
    loginReply = loginmessage;
  } else if (data.at(1).type == msgpack::type::MAP) {
    errorDetails = data.at(1).as<std::map<int32_t, std::string>>();
  }

  if (!data.at(2).is_nil()) {
    ackException = data.at(2).as<std::string>();
  } else {
    ackException.reset();
  }
  validate();
}

void GdsLoginReplyMessage::validate() const {
  if (loginReply.has_value() == errorDetails.has_value()) {
    if (loginReply.has_value()) {
      throw invalid_message_error(type());
    }
  }
}

/*2*/
void GdsEventMessage::pack(msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(3);
  packer.pack(operations);
  packer.pack(binaryContents);
  packer.pack(priorityLevels);
}
void GdsEventMessage::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();
  operations = data.at(0).as<std::string>();
  binaryContents = data.at(1).as<std::map<int32_t, byte_array>>();
  priorityLevels =
      data.at(2).as<std::vector<std::vector<std::map<int32_t, bool>>>>();
  validate();
}

void GdsEventMessage::validate() const {
  // skip, nothing specific here
}

/*3*/
void GdsEventReplyMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();

  packer.pack_array(3);
  packer.pack_int32(ackStatus);
  if (reply) {
    reply->pack(packer);
  } else {
    packer.pack_nil();
  }
  if (ackException) {
    packer.pack(ackException);
  } else {
    packer.pack_nil();
  }
}

void GdsEventReplyMessage::unpack(const msgpack::object &packer) {

  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();
  ackStatus = data.at(0).as<int32_t>();
  if (!data.at(1).is_nil()) {
    EventReplyBody body;
    body.unpack(data.at(1));
    reply = body;
  } else {
    reply.reset();
  }
  if (!data.at(2).is_nil()) {
    ackException = data.at(2).as<std::string>();
  } else {
    ackException.reset();
  }
  validate();
}

void GdsEventReplyMessage::validate() const {
  // skip
}

/*4*/
void GdsAttachmentRequestMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack(request);
}
void GdsAttachmentRequestMessage::unpack(const msgpack::object &obj) {
  request = obj.as<std::string>();
  validate();
}
void GdsAttachmentRequestMessage::validate() const {
  // skip
}

/*5*/
void GdsAttachmentRequestReplyMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(3);
  packer.pack_int32(ackStatus);
  if (ackStatus == GDSStatusCodes::OK) {
    request->pack(packer);
  } else {
    packer.pack_nil();
  }
  if (ackException) {
    packer.pack(ackException);
  } else {
    packer.pack_nil();
  }
}

void GdsAttachmentRequestReplyMessage::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();
  ackStatus = data.at(0).as<int32_t>();
  if (!data.at(1).is_nil()) {
    request->unpack(data.at(1));
  } else {
    request.reset();
  }
  if (!data.at(2).is_nil()) {
    ackException = data.at(2).as<std::string>();
  } else {
    ackException.reset();
  }

  validate();
}
void GdsAttachmentRequestReplyMessage::validate() const {
  if ((ackStatus == GDSStatusCodes::OK) != (request.has_value())) {
    throw invalid_message_error(type());
  }
}

/*6*/
void GdsAttachmentResponseMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(1);
  result.pack(packer);
}

void GdsAttachmentResponseMessage::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> obj = packer.as<std::vector<msgpack::object>>();
  result.unpack(obj.at(0));
  validate();
}
void GdsAttachmentResponseMessage::validate() const { result.validate(); }

/*7*/
void GdsAttachmentResponseResultMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(3);
  packer.pack_int32(ackStatus);

  if (response) {
    response->pack(packer);
  } else {
    packer.pack_nil();
  }

  if (ackException) {
    packer.pack(ackException);
  } else {
    packer.pack_nil();
  }
}

void GdsAttachmentResponseResultMessage::unpack(const msgpack::object &packer) {

  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();
  ackStatus = data.at(0).as<int32_t>();
  if (!data.at(1).is_nil()) {
    AttachmentResponseBody body;
    body.unpack(data.at(1));
    response = body;
  } else {
    response.reset();
  }

  if (!data.at(2).is_nil()) {
    ackException = data.at(2).as<std::string>();
  } else {
    ackException.reset();
  }
  validate();
}
void GdsAttachmentResponseResultMessage::validate() const {
  if (response) {
    response->validate();
  }
}

/*8*/
void GdsEventDocumentMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(4);
  packer.pack(tableName);
  packer.pack_array(fieldDescriptors.size());
  for (auto &item : fieldDescriptors) {
    packer.pack_array(3);
    for (auto &desc : item) {
      packer.pack(desc);
    }
  }

  packer.pack_array(records.size());
  for (auto &hit_rows : records) {
    packer.pack_array(hit_rows.size());
    for (auto &hit : hit_rows) {
      hit.pack(packer);
    }
  }

  packer.pack_map(0);
}

void GdsEventDocumentMessage::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> obj = packer.as<std::vector<msgpack::object>>();
  tableName = obj.at(0).as<std::string>();

  std::vector<msgpack::object> fielddescriptors =
      obj.at(1).as<std::vector<msgpack::object>>();
  fieldDescriptors.reserve(fielddescriptors.size());
  for (auto &item : fielddescriptors) {
    std::vector<std::string> data = item.as<std::vector<std::string>>();
    field_descriptor         desc;
    desc[0] = data.at(0);
    desc[1] = data.at(1);
    desc[2] = data.at(2);
    fieldDescriptors.emplace_back(desc);
  }

  std::vector<msgpack::object> values =
      obj.at(2).as<std::vector<msgpack::object>>();
  records.reserve(values.size());
  for (auto &object : values) {
    std::vector<msgpack::object> row =
        object.as<std::vector<msgpack::object>>();
    std::vector<GdsFieldValue> currenthit;
    currenthit.reserve(row.size());
    for (auto &val : row) {
      GdsFieldValue value;
      value.unpack(val);
      currenthit.emplace_back(value);
    }

    records.emplace_back(currenthit);
  }

  // returnings = obj.at(3).as<std::map<int32_t, std::vector<std::string>>>();
  validate();
}
void GdsEventDocumentMessage::validate() const {
  size_t headerCount = fieldDescriptors.size();
  for (auto &row : records) {
    if (headerCount != row.size()) {
      throw invalid_message_error(type());
    }
  }
}

/*9*/
void GdsEventDocumentReplyMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();

  packer.pack_array(3);
  packer.pack_int32(ackStatus);
  if (results) {
    packer.pack_array(results->size());
    for (auto &event : results.value()) {
      event.pack(packer);
    }
  } else {
    packer.pack_nil();
  }

  if (ackException) {
    packer.pack(ackException);
  } else {
    packer.pack_nil();
  }
}

void GdsEventDocumentReplyMessage::unpack(const msgpack::object &packer) {

  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();
  ackStatus = data.at(0).as<int32_t>();
  if (!data.at(1).is_nil()) {
    std::vector<msgpack::object> items =
        data.at(1).as<std::vector<msgpack::object>>();
    std::vector<EventDocumentResult> _results;
    _results.reserve(items.size());
    for (auto &object : items) {
      EventDocumentResult result;
      result.unpack(object);
      _results.emplace_back(result);
    }
    results = _results;
  } else {
    results.reset();
  }

  if (!data.at(2).is_nil()) {
    ackException = data.at(2).as<std::string>();
  } else {
    ackException.reset();
  }
  validate();
}
void GdsEventDocumentReplyMessage::validate() const {
  // skip
}

/*10*/
void GdsQueryRequestMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  if (queryPageSize.has_value() && queryType.has_value()) {
    packer.pack_array(5);
  } else {
    packer.pack_array(3);
  }
  packer.pack(selectString);
  packer.pack(consistency);
  packer.pack_int64(timeout);

  if (queryPageSize.has_value() && queryType.has_value()) {
    packer.pack_int32(queryPageSize.value());
    packer.pack_int32(queryType.value());
  }
}

void GdsQueryRequestMessage::unpack(const msgpack::object &obj) {
  std::vector<msgpack::object> items = obj.as<std::vector<msgpack::object>>();
  selectString = items.at(0).as<std::string>();
  consistency = items.at(1).as<std::string>();
  timeout = items.at(2).as<int64_t>();
  if (items.size() == 5) {
    queryPageSize = items.at(3).as<int32_t>();
    queryType = items.at(4).as<int32_t>();
  }
  validate();
}

void GdsQueryRequestMessage::validate() const {
  if (queryPageSize.has_value() != queryType.has_value()) {
    throw invalid_message_error(type());
  }
}

/*11*/
void GdsQueryReplyMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(3);
  packer.pack_int32(ackStatus);
  if (response) {
    response->pack(packer);
  } else {
    packer.pack_nil();
  }

  if (ackException) {
    packer.pack(ackException);
  } else {
    packer.pack_nil();
  }
}

void GdsQueryReplyMessage::unpack(const msgpack::object &packer) {
  std::vector<msgpack::object> data = packer.as<std::vector<msgpack::object>>();
  ackStatus = data.at(0).as<int32_t>();

  if (!data.at(1).is_nil()) {
    QueryReplyBody body;
    body.unpack(data.at(1));
    response = body;
  } else {
    response.reset();
  }

  if (!data.at(2).is_nil()) {
    ackException = data.at(2).as<std::string>();
  } else {
    ackException.reset();
  }
  validate();
}
void GdsQueryReplyMessage::validate() const {
  if (response) {
    response->validate();
  }
}

/*12*/
void GdsNextQueryRequestMessage::pack(
    msgpack::packer<msgpack::sbuffer> &packer) const {
  validate();
  packer.pack_array(2);
  contextDescriptor.pack(packer);
  packer.pack_int64(timeout);
}

void GdsNextQueryRequestMessage::unpack(const msgpack::object &obj) {
  std::vector<msgpack::object> items = obj.as<std::vector<msgpack::object>>();
  contextDescriptor.unpack(items.at(0));
  timeout = items.at(1).as<int64_t>();
  validate();
}
void GdsNextQueryRequestMessage::validate() const {
  // skip
}

} // namespace gds_types
} // namespace gds_lib