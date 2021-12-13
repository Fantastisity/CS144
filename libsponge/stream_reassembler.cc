#include "stream_reassembler.hh"

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), pq() {}

void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (ind > index + data.size()) return;
    if (eof_yet > -1 && static_cast<int>(index) >= eof_yet) return;
    
    bool _eof = eof; size_t len, fin, index_t = index; string sub;
    size_t capacity = _capacity + (ind > _output.buffer_size() ? ind - _output.buffer_size() : 0);
    
    if (data.size() + index > capacity) _eof = 0, len = capacity - index;
    else len = data.size();
    
    if (index_t < ind) sub = data.substr(ind - index), len -= (ind - index), index_t = ind;
    else sub = data.substr(0, len);
    
    fin = index_t + len;
    if (_eof) eof_yet = static_cast<int>(fin);

    if (index_t == ind) ind += len, _output.write(sub);
    else {
        int f = pq.count(index_t), f1 = 0;
        if (!f || (f1 = (f && pq[index_t].first < fin))) {
            unassem += fin - (f1 ? pq[index_t].first : index_t);
            for (auto iter = pq.begin(), ed = pq.end(); iter != ed;) {
                if (iter->first > index_t && iter->second.first < fin) {
                    unassem -= iter->second.second.size();
                    pq.erase(iter++);
                    continue;
                } 
                if (iter->first < index_t && iter->second.first > fin) {
                    unassem -= sub.size();
                    f = 2;
                } 
                ++iter;
            }
            if (f < 2) pq[index_t] = mp(fin, sub);
        }
    }
    
    for(;ind;) {
        auto iter = pq.lower_bound(ind);
        if (iter != pq.end() && iter->first == ind) sub = iter->second.second;
        else if (iter != pq.begin()){
            --iter;
            if (iter->second.first <= ind) {
                unassem -= iter->second.second.size();
                pq.erase(iter);
                continue;
            }
            sub = iter->second.second.substr(ind - iter->first);        
        } else break;
          pq.erase(iter);
          ind = iter->second.first; 
          unassem -= sub.size();
          _output.write(sub);
    }
    if (eof_yet > -1 && static_cast<int>(ind) >= eof_yet) _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return unassem; }

bool StreamReassembler::empty() const { return !unassem; }
