#ifndef FIRST_FOLLOW_H
#define FIRST_FOLLOW_H

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "grammar.h"

class FirstFollow {
public:
    static const std::string EPS;

    explicit FirstFollow(const Grammar& g);

    const std::set<std::string>& first(const std::string& symbol) const;
    const std::set<std::string>& follow(const std::string& symbol) const;
    bool nullable(const std::string& symbol) const;

    std::set<std::string> firstOfSequence(const std::vector<std::string>& sequence) const;

    void printFirst(std::ostream& os) const;
    void printFollow(std::ostream& os) const;

private:
    void computeNullable();
    void computeFirst();
    void computeFollow();

    const Grammar& g_;
    std::set<std::string> nullable_;
    std::map<std::string, std::set<std::string> > first_;
    std::map<std::string, std::set<std::string> > follow_;
};

#endif
