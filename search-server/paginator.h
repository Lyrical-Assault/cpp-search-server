#pragma once
#include <iostream>
#include <algorithm>

template <typename Iterator>
class IteratorRange{
public:
    IteratorRange(Iterator begin, Iterator end)
            : begin_(begin), end_(end), size_(distance(begin_, end_)){
    }

    Iterator begin() const{
        return begin_;
    }

    Iterator end() const{
        return end_;
    }

    size_t size() {
        return size_;
    }

private:
    Iterator begin_, end_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<< (std::ostream& out, IteratorRange<Iterator> iteratorRange){
    for(auto iter = iteratorRange.begin(); iter != iteratorRange.end(); ++iter){
        out << *iter;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) : paginator_() {
        for(Iterator iter = begin; iter != end;){
            const size_t dist = distance(iter, end);
            const size_t current_page_size = std::min(page_size, dist);
            IteratorRange page(iter, iter + current_page_size);
            paginator_.push_back(page);
            advance(iter, current_page_size);
        }
    }

    typename std::vector<IteratorRange<Iterator>>::const_iterator begin() const{
        return paginator_.begin();
    }

    typename std::vector<IteratorRange<Iterator>>::const_iterator end() const{
        return paginator_.end();
    }

private:
    std::vector<IteratorRange<Iterator>> paginator_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}