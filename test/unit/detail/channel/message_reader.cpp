//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/channel/message_reader.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

/**
process_message
    fragmented msg 2
        header
        body
    fragmented msg 3
        header + body part
        end of body + next header + next body part 
    full message: header + body
    several messages, then fragmented one
        header 1 + body 1 + header 2 + body 2 + header 3 + body fragment 3
    several messages
        header 1 + body 1 + header 2 + body 2
    2-frame message
        header 1 + body 1 part
        body 1 part
        body 1 part + header 2 + body 2 part
        body 2 part
    3-frame message
        header 1 + body 1 part
        body 1 part
        body 1 part + header 2 + body 2 part
        body 2 part
        body 2 part + header 3 + body 3
    2-frame message in single read (not possible?)
        header 1 + body 1 + header 2 + body 2
    2-frame message with fragmented header
        header 1 piece
        header 1 piece + body 1 piece
        body 1 piece + header 2 piece
    coming from an already processed message (can we get rid of this?)
    coming from an error (can we get rid of this?)
    with reserved area
    2-frame message with mismatched seqnums
    2 different frames with "mismatched" seqnums

next_message
    passed number seqnum mismatch
    intermediate frame seqnum mismatch
    OK, there is next message
    OK, there isn't next message
read_some
    has already a message
    message that fits in the buffer
    message that fits in the buffer, but segmented in two
    message that doesn't fit in the buffer (segmented)
    there is a previous message, keep_messages = false
    there is a previous message, keep_messages = true
    error in a read
read_one
    

 * 
 */

namespace
{




}