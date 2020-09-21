#ifndef MyersDiff_HPP
#define MyersDiff_HPP

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <vector>

using namespace std;

enum Operation : int8_t
{
  EQUAL = 0,
  INSERT = 1,
  DELETE = 2
};

inline char op2chr(Operation op)
{
  switch (op)
  {
  case DELETE:
    return '-';
  case INSERT:
    return '+';
  case EQUAL:
    return '=';
  default:
    return '?';
  }
}

/*
 * Computes the difference between two texts to create a patch.
 * Also contains the behaviour settings.
 */
template <class String>
class MyersDiff
{
public:
  // Defaults.
  // Set these on your diff_match_patch instance to override the defaults.

  /**
     * Number of milliseconds to map a diff before giving up (0 for infinity).
     */
  long Diff_Timeout = 100;
  /**
     * Cost of an empty edit operation in terms of edit characters.
     */
  uint16_t Diff_EditCost = 4;
  /**
     * At what point is no match declared (0.0 = perfection, 1.0 = very loose).
     */
  float Match_Threshold = 0.5f;
  /**
     * How far to search for a match (0 = exact location, 1000+ = broad match).
     * A match this many characters away from the expected location will add
     * 1.0 to the score (0.0 is a perfect match).
     */
  int Match_Distance = 500;
  /**
     * When deleting a large block of text (over ~64 characters), how close do
     * the contents have to be to match the expected contents. (0.0 =
     * perfection, 1.0 = very loose).  Note that Match_Threshold controls how
     * closely the end points of a delete need to match.
     */
  float Patch_DeleteThreshold = 0.5f;
  /**
     * Chunk size for context length.
     */
  uint16_t Patch_Margin = 4;

public:
  using Char = typename String::value_type;
  using Time = chrono::time_point<chrono::steady_clock>;
  using Size = typename String::size_type;
  using ConstIter = typename String::const_iterator;

  struct Range
  {
    ConstIter from, till;
    Range(ConstIter begin, ConstIter end) : from{begin}, till{end} {}
    explicit Range(const String &str)
        : from{str.begin()}, till{str.end()} {}
    bool operator==(Range b) const
    {
      if (till - from != b.till - b.from)
        return false;
      for (auto i = from, j = b.from; i < till; ++i, ++j)
        if (*i != *j)
          return false;
      return true;
    }
    Size size() const { return till - from; }
    Range substr(Size start) const
    {
      assert(start <= size());
      return Range{from + start, till};
    }
    Range substr(Size start, Size end) const
    {
      assert(end >= start);
      assert(start <= size());
      if (end >= size())
        end = size();
      return Range{from + start, from + end};
    }
    Char operator[](Size idx) const
    {
      assert(idx < size());
      return *(from + idx);
    }
    Size find(Range b) const
    {
      auto at = std::search(from, till, b.from, b.till);
      return at < till ? at - from : -1;
    }
  };

  struct Diff
  {
    Operation operation;
    Range text;
    Diff(Operation op, Range text_range)
        : operation{op}, text{text_range} {}
    std::string str() const
    {
      string ret;
      // ret.push_back(op2chr(operation));
      // ret.push_back('\t');
      ret.append(text.from, text.till);
      return ret;
    }
  };

  using Diffs = std::vector<Diff>;

private:
  const String &text1;
  const String &text2;
  Diffs result;

public:
  MyersDiff(const String &original_text, const String &changed_text)
      : text1{original_text}, text2{changed_text}
  {
    result = diff_main(Range{text1}, Range{text2});
  }

  typename Diffs::const_iterator begin() const { return result.begin(); }
  typename Diffs::const_iterator end() const { return result.end(); }

  const Diffs &diffs() const { return result; }

  //  DIFF FUNCTIONS

  /**
     * Find the differences between two texts.
     * @return std::vector of Diff objects.
     */
  Diffs diff_main(Range text1, Range text2)
  {
    // Set a deadline by which time the diff must be complete.
    Time deadline;
    if (Diff_Timeout <= 0)
    {
      deadline = Time::max();
    }
    else
    {
      deadline = chrono::steady_clock::now() +
                 chrono::milliseconds(Diff_Timeout);
    }
    return diff_main(text1, text2, deadline);
  }

  /**
     * Find the differences between two texts.  Simplifies the problem by
     * stripping any common prefix or suffix off the texts before diffing.
     * @param deadline Time when the diff should be complete by.  Used
     *     internally for recursive calls.  Users should set DiffTimeout
     * instead.
     * @return std::vector of Diff objects.
     */
  Diffs diff_main(Range text1, Range text2, Time deadline)
  {
    // Check for equality (speedup).
    Diffs diffs{};
    if (text1 == text2)
    {
      if (text1.size() != 0)
      {
        diffs.push_back(Diff(EQUAL, text1));
      }
      return diffs;
    }

    // Trim off common prefix (speedup).
    int commonlength = diff_commonPrefix(text1, text2);
    Range commonprefix = text1.substr(0, commonlength);
    text1 = text1.substr(commonlength);
    text2 = text2.substr(commonlength);

    // Trim off common suffix (speedup).
    commonlength = diff_commonSuffix(text1, text2);
    Range commonsuffix = text1.substr(text1.size() - commonlength);
    text1 = text1.substr(0, text1.size() - commonlength);
    text2 = text2.substr(0, text2.size() - commonlength);

    // Compute the diff on the middle block.
    diffs = diff_compute(text1, text2, deadline);

    // Restore the prefix and suffix.
    if (commonprefix.size() != 0)
    {
      diffs.insert(diffs.begin(), Diff(EQUAL, commonprefix));
    }
    if (commonsuffix.size() != 0)
    {
      diffs.push_back(Diff(EQUAL, commonsuffix));
    }

    // TODO diff_cleanupMerge(diffs);
    return diffs;
  }

  /**
     * Find the differences between two texts.  Assumes that the texts do not
     * have any common prefix or suffix.
     * @param text1 Old string to be diffed.
     * @param text2 New string to be diffed.
     * @param checklines Speedup flag.  If false, then don't run a
     *     line-level diff first to identify the changed areas.
     *     If true, then run a faster slightly less optimal diff.
     * @param deadline Time when the diff should be complete by.
     * @return std::vector of Diff objects.
     */
  Diffs diff_compute(Range text1, Range text2, Time deadline)
  {
    Diffs diffs{};

    if (text1.size() == 0)
    {
      // Just add some text (speedup).
      diffs.push_back(Diff(INSERT, text2));
      return diffs;
    }

    if (text2.size() == 0)
    {
      // Just delete some text (speedup).
      diffs.push_back(Diff(DELETE, text1));
      return diffs;
    }

    Range longtext = text1.size() > text2.size() ? text1 : text2;
    Range shorttext = text1.size() > text2.size() ? text2 : text1;
    int i = longtext.find(shorttext);
    if (i != -1)
    {
      // Shorter text is inside the longer text (speedup).
      Operation op = (text1.size() > text2.size()) ? DELETE : INSERT;
      diffs.push_back(Diff(op, longtext.substr(0, i)));
      diffs.push_back(Diff(EQUAL, shorttext));
      diffs.push_back(Diff(op, longtext.substr(i + shorttext.size())));
      return diffs;
    }

    if (shorttext.size() == 1)
    {
      // Single character string.
      // After the previous speedup, the character can't be an equality.
      diffs.push_back(Diff(DELETE, text1));
      diffs.push_back(Diff(INSERT, text2));
      return diffs;
    }

    // Check to see if the problem can be split in two.
    /* TODO String[] hm = diff_halfMatch(text1, text2);
        if (hm != null) {
          // A half-match was found, sort out the return data.
          Range text1_a = hm[0];
          Range text1_b = hm[1];
          Range text2_a = hm[2];
          Range text2_b = hm[3];
          String mid_common = hm[4];
          // Send both pairs off for separate processing.
          Diffs diffs_a = diff_main(text1_a, text2_a,
                                               checklines, deadline);
          Diffs diffs_b = diff_main(text1_b, text2_b,
                                               checklines, deadline);
          // Merge the results.
          diffs = diffs_a;
          diffs.push_back(Diff(EQUAL, mid_common));
          diffs.addAll(diffs_b);
          return diffs;
        }

        if (checklines && text1.size() > 100 && text2.size() > 100) {
          return diff_lineMode(text1, text2, deadline);
        }*/

    return diff_bisect(text1, text2, deadline);
  }

  /**
     * Find the 'middle snake' of a diff, split the problem in two
     * and return the recursively constructed diff.
     * See Myers 1986 paper: An O(ND) Difference Algorithm and Its Variations.
     * @param text1 Old string to be diffed.
     * @param text2 New string to be diffed.
     * @param deadline Time at which to bail if not yet complete.
     * @return std::vector of Diff objects.
     */
  Diffs diff_bisect(Range text1, Range text2, Time deadline)
  {
    // Cache the text lengths to prevent multiple calls.
    int text1_length = text1.size();
    int text2_length = text2.size();
    int max_d = (text1_length + text2_length + 1) / 2;
    int v_offset = max_d;
    int v_length = 2 * max_d;
    vector<int> v1;
    v1.resize(v_length);
    vector<int> v2;
    v2.resize(v_length);
    for (int x = 0; x < v_length; x++)
    {
      v1[x] = -1;
      v2[x] = -1;
    }
    v1[v_offset + 1] = 0;
    v2[v_offset + 1] = 0;
    int delta = text1_length - text2_length;
    // If the total number of characters is odd, then the front path will
    // collide with the reverse path.
    bool front = (delta % 2 != 0);
    // Offsets for start and end of k loop.
    // Prevents mapping of space beyond the grid.
    int k1start = 0;
    int k1end = 0;
    int k2start = 0;
    int k2end = 0;
    for (int d = 0; d < max_d; d++)
    {
      // Bail out if deadline is reached.
      /* TODO if (System.currentTimeMillis() > deadline) {
              break;
            } */

      // Walk the front path one step.
      for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2)
      {
        int k1_offset = v_offset + k1;
        int x1;
        if (k1 == -d ||
            (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1]))
        {
          x1 = v1[k1_offset + 1];
        }
        else
        {
          x1 = v1[k1_offset - 1] + 1;
        }
        int y1 = x1 - k1;
        while (x1 < text1_length && y1 < text2_length &&
               text1[x1] == text2[y1])
        {
          x1++;
          y1++;
        }
        v1[k1_offset] = x1;
        if (x1 > text1_length)
        {
          // Ran off the right of the graph.
          k1end += 2;
        }
        else if (y1 > text2_length)
        {
          // Ran off the bottom of the graph.
          k1start += 2;
        }
        else if (front)
        {
          int k2_offset = v_offset + delta - k1;
          if (k2_offset >= 0 && k2_offset < v_length &&
              v2[k2_offset] != -1)
          {
            // Mirror x2 onto top-left coordinate system.
            int x2 = text1_length - v2[k2_offset];
            if (x1 >= x2)
            {
              // Overlap detected.
              return diff_bisectSplit(text1, text2, x1, y1,
                                      deadline);
            }
          }
        }
      }

      // Walk the reverse path one step.
      for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2)
      {
        int k2_offset = v_offset + k2;
        int x2;
        if (k2 == -d ||
            (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1]))
        {
          x2 = v2[k2_offset + 1];
        }
        else
        {
          x2 = v2[k2_offset - 1] + 1;
        }
        int y2 = x2 - k2;
        while (x2 < text1_length && y2 < text2_length &&
               text1[text1_length - x2 - 1] ==
                   text2[text2_length - y2 - 1])
        {
          x2++;
          y2++;
        }
        v2[k2_offset] = x2;
        if (x2 > text1_length)
        {
          // Ran off the left of the graph.
          k2end += 2;
        }
        else if (y2 > text2_length)
        {
          // Ran off the top of the graph.
          k2start += 2;
        }
        else if (!front)
        {
          int k1_offset = v_offset + delta - k2;
          if (k1_offset >= 0 && k1_offset < v_length &&
              v1[k1_offset] != -1)
          {
            int x1 = v1[k1_offset];
            int y1 = v_offset + x1 - k1_offset;
            // Mirror x2 onto top-left coordinate system.
            x2 = text1_length - x2;
            if (x1 >= x2)
            {
              // Overlap detected.
              return diff_bisectSplit(text1, text2, x1, y1,
                                      deadline);
            }
          }
        }
      }
    }
    // Diff took too long and hit the deadline or
    // number of diffs equals number of characters, no commonality at all.
    Diffs diffs{};
    diffs.push_back(Diff{DELETE, text1});
    diffs.push_back(Diff{INSERT, text2});
    return diffs;
  }

  /**
     * Given the location of the 'middle snake', split the diff in two parts
     * and recurse.
     * @param text1 Old string to be diffed.
     * @param text2 New string to be diffed.
     * @param x Index of split point in text1.
     * @param y Index of split point in text2.
     * @param deadline Time at which to bail if not yet complete.
     * @return std::vector of Diff objects.
     */
  Diffs diff_bisectSplit(Range text1, Range text2, int x, int y,
                         Time deadline)
  {
    Range text1a = text1.substr(0, x);
    Range text2a = text2.substr(0, y);
    Range text1b = text1.substr(x);
    Range text2b = text2.substr(y);

    // Compute both diffs serially.
    Diffs diffs = diff_main(text1a, text2a, deadline);
    Diffs diffsb = diff_main(text1b, text2b, deadline);

    diffs.insert(diffs.end(), diffsb.begin(), diffsb.end());
    return diffs;
  }

  /**
     * Determine the common prefix of two strings
     * @param text1 First string.
     * @param text2 Second string.
     * @return The number of characters common to the start of each string.
     */
  int diff_commonPrefix(Range text1, Range text2)
  {
    // Performance analysis: https://neil.fraser.name/news/2007/10/09/
    int n = std::min(text1.size(), text2.size());
    for (int i = 0; i < n; i++)
    {
      if (text1[i] != text2[i])
      {
        return i;
      }
    }
    return n;
  }

  /**
     * Determine the common suffix of two strings
     * @param text1 First string.
     * @param text2 Second string.
     * @return The number of characters common to the end of each string.
     */
  int diff_commonSuffix(Range text1, Range text2)
  {
    // Performance analysis: https://neil.fraser.name/news/2007/10/09/
    int text1_length = text1.size();
    int text2_length = text2.size();
    int n = std::min(text1_length, text2_length);
    for (int i = 1; i <= n; i++)
    {
      if (text1[text1_length - i] != text2[text2_length - i])
      {
        return i - 1;
      }
    }
    return n;
  }

  /**
     * Determine if the suffix of one string is the prefix of another.
     * @param text1 First string.
     * @param text2 Second string.
     * @return The number of characters common to the end of the first
     *     string and the start of the second string.
     */
  int diff_commonOverlap(Range text1, Range text2)
  {
    // Cache the text lengths to prevent multiple calls.
    int text1_length = text1.size();
    int text2_length = text2.size();
    // Eliminate the null case.
    if (text1_length == 0 || text2_length == 0)
    {
      return 0;
    }
    // Truncate the longer string.
    if (text1_length > text2_length)
    {
      text1 = text1.substr(text1_length - text2_length);
    }
    else if (text1_length < text2_length)
    {
      text2 = text2.substr(0, text1_length);
    }
    int text_length = std::min(text1_length, text2_length);
    // Quick check for the worst case.
    if (text1 == text2)
    {
      return text_length;
    }

    // Start by looking for a single character match
    // and increase length until no match is found.
    // Performance analysis: https://neil.fraser.name/news/2010/11/04/
    int best = 0;
    int length = 1;
    while (true)
    {
      String pattern = text1.substr(text_length - length);
      int found = text2.indexOf(pattern);
      if (found == -1)
      {
        return best;
      }
      length += found;
      if (found == 0 ||
          text1.substr(text_length - length) == text2.substr(0, length))
      {
        best = length;
        length++;
      }
    }
  }

  /**
     * Compute and return the source text (all equalities and deletions).
     * @param diffs std::vector of Diff objects.
     * @return Source text.
     */
  String diff_text1(Diffs diffs)
  {
    Range text{};
    for (Diff aDiff : diffs)
    {
      if (aDiff.operation != INSERT)
      {
        text.append(aDiff.text);
      }
    }
    return text;
  }

  /**
     * Compute and return the destination text (all equalities and insertions).
     * @param diffs std::vector of Diff objects.
     * @return Destination text.
     */
  String diff_text2(Diffs diffs)
  {
    Range text{};
    for (Diff aDiff : diffs)
    {
      if (aDiff.operation != DELETE)
      {
        text.append(aDiff.text);
      }
    }
    return text;
  }

  struct Stats
  {
    Size equal, inserted, deleted;
    Stats() : equal{0}, inserted{0}, deleted{0} {}
  };

  Stats stats() const
  {
    Stats ret;
    for (const auto &i : result)
    {
      switch (i.operation)
      {
      case EQUAL:
        ret.equal += i.text.size();
        break;
      case INSERT:
        ret.inserted += i.text.size();
        break;
      case DELETE:
        ret.deleted += i.text.size();
        break;
      }
    }
    return ret;
  }
};

#endif
