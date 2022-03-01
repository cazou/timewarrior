////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 - 2021, Thomas Lauf, Paul Beckingham, Federico Hernandez.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#include <Table.h>
#include <Duration.h>
#include <shared.h>
#include <format.h>
#include <commands.h>
#include <timew.h>
#include <iostream>
#include <IntervalFilterAndGroup.h>
#include <IntervalFilterAllWithTags.h>
#include <IntervalFilterAllInRange.h>

// Implemented in CmdChart.cpp.
std::map <Datetime, std::string> createHolidayMap (Rules&, Interval&);
std::string renderHolidays2 (const std::map <Datetime, std::string>&);

class TimeSheetEntry {
private:
    std::map<std::string, std::vector<Interval>> m_track_tags;

public:
    TimeSheetEntry() {
    }

    virtual bool isCategoryTag(const std::string& tag) const = 0;

    virtual std::string category() const = 0;
    virtual std::string uri() const = 0;
    virtual std::string prettyId() const = 0;

    void add_track_tags(const std::set<std::string> &tags, const Interval &track) {
        std::string t = "";
        for (auto& tag : tags) {
            auto tag_l = lowerCase(tag);
            if (isCategoryTag(tag_l))
                continue;

            if (!t.empty())
                t.append(", ");

            t.append(upperCaseFirst(tag_l));
        }

        m_track_tags[t].push_back(track);
    }

    std::map<std::string, std::vector<Interval>> track_tags() const {
        return m_track_tags;
    }

};

class Guild: public TimeSheetEntry {
public:
    Guild() {
    }

    static bool isGuildTag(const std::string& tag) {
        return tag.rfind("guild", 0) == 0;
    }

    virtual bool isCategoryTag(const std::string& tag) const {
        return isGuildTag(tag);
    }

    std::string category() const {
        return "guild";
    }

    std::string uri() const {
        return "";
    }

    virtual std::string prettyId() const {
        return "Guild";
    }
};

class Standup: public TimeSheetEntry {
public:
    Standup() {
    }

    static bool isStandupTag(const std::string& tag) {
        return tag.rfind("standup", 0) == 0;
    }

    virtual bool isCategoryTag(const std::string& tag) const {
        return isStandupTag(tag);
    }

    std::string category() const {
        return "standup";
    }

    std::string uri() const {
        return "";
    }

    virtual std::string prettyId() const {
        return "Standup";
    }
};

class Induction: public TimeSheetEntry {
public:
    Induction() {
    }

    static bool isInductionTag(const std::string& tag) {
        return tag.rfind("guild", 0) == 0;
    }

    virtual bool isCategoryTag(const std::string& tag) const {
        return isInductionTag(tag);
    }

    std::string category() const {
        return "induction";
    }

    std::string uri() const {
        return "";
    }

    virtual std::string prettyId() const {
        return "Induction";
    }
};

class PhabricatorTask: public TimeSheetEntry {
private:
    std::string             m_taskId;
    std::string             m_title;
    std::set<std::string>   m_tags;

public:
    PhabricatorTask(const std::string &taskId)
        : m_taskId(taskId) {
    }

    /*
     * Fetch the data of the task from phabricator
     * //TODO: Error enum
     */
    int fetch() {
        // TODO
        m_title = "TITLE";
        m_tags.insert("BOS9");
        m_tags.insert("bug");

        return 0;
    }

    /*
     * Return the tags of the task
     */
    std::set<std::string> tags() const {
        return m_tags;
    }

    /*
     * Return the title of the task
     */
    std::string title() const {
        return m_title;
    }

    /*
     * Return the id of the task.
     * Note that IDs can be in 2 formats:
     * * T[09]+ -> task from phabricator.collabora.com
     * * AT[09]+ -> task from phabricator.apertis.com
     */
    std::string id()const {
        return m_taskId;
    }

    int nid() const {
        if (m_taskId.rfind("at", 0) == 0)
            return atoi(m_taskId.c_str() + 2);
        if (m_taskId.rfind("t", 0) == 0)
            return atoi(m_taskId.c_str() + 1);

        return -1;
    }

    std::string category() const {
        return id();
    }

    std::string uri() const {
        if (m_taskId.rfind("at", 0) == 0 && atoi(m_taskId.c_str() + 2) != 0)
            return format("https://phabricator.apertis.org/T{1}", m_taskId.c_str() + 2);
        else if (m_taskId.rfind("t", 0) == 0 && atoi(m_taskId.c_str() + 1) != 0)
            return format("https://phabricator.collabora.org/T{1}", m_taskId.c_str() + 1);

        return "";
    }

    static bool isTaskTag(const std::string &tag) {
        return (tag.rfind("at", 0) == 0 && atoi(tag.c_str() + 2) != 0) ||
                (tag.rfind("t", 0) == 0 && atoi(tag.c_str() + 1) != 0);
    }

    virtual bool isCategoryTag(const std::string &tid) const {
        return isTaskTag(tid);
    }

    virtual std::string prettyId() const {
        return upperCase(m_taskId);
    }
};

bool isLess(TimeSheetEntry* a, TimeSheetEntry* b) {
    PhabricatorTask *ta = dynamic_cast<PhabricatorTask*>(a);
    PhabricatorTask *tb = dynamic_cast<PhabricatorTask*>(b);

    if (ta && tb)
        return ta->nid() < tb->nid();

    return false;
}

std::vector<TimeSheetEntry*> _build_entries(Datetime day, std::vector<Interval> tracked) {
    std::vector<TimeSheetEntry*> entries;
    auto day_range = getFullDay (day);

    for (auto& track : subset (day_range, tracked))
    {
        // Make sure the track only represents one day.
        if ((track.is_open () && day > Datetime ()))
            continue;

        if (track.empty())
            continue;

        // TODO: Manage tasks on multiple days (although this is not recommendend behavior @Collabora)

        // Check if we have a Task or known category tag (Standup, Induction, Guild)
        for (auto& tag : track.tags()) {
            // Check the tags to find the track type
            TimeSheetEntry *entry = NULL;
            auto tag_l = lowerCase(tag);

            for (auto& tse : entries) {
                if (tse->category() == tag_l) {
                    entry = tse;
                    break;
                }
            }

            if (entry == NULL) {
                TimeSheetEntry *new_entry;

                if (PhabricatorTask::isTaskTag(tag_l)) {
                    new_entry = new PhabricatorTask(tag_l);
                } else if (Guild::isGuildTag(tag_l)) {
                    new_entry = new Guild();
                } else if (Standup::isStandupTag(tag_l)) {
                    new_entry = new Standup();
                } else if (Induction::isInductionTag(tag_l)) {
                    new_entry = new Induction();
                } else {
                    continue;
                }

                new_entry->add_track_tags(track.tags(), track);
                entries.push_back(new_entry);
            } else {
                entry->add_track_tags(track.tags(), track);
            }
        }
    }

    return entries;
}

void _table_init(Table *table) {
    table->width (1024);
    table->colorHeader (Color ("underline"));
    table->add ("Wk");
    table->add ("Date");
    table->add ("Day");
    table->add ("Category");
    table->add ("Tags");
    table->add ("Phabricator");
    table->add ("IDs");

    table->add ("Time", false);
    table->add ("Total", false);
}

uint _table_add_entries(Table *table, int* last_row, const std::vector<TimeSheetEntry*>& entries, Color colorID) {
    int row = *last_row;
    int daily_total = 0;

    for (auto& entry : entries) {
        time_t entry_time = 0;

        table->set (row, 3, entry->prettyId());
        auto uri_set = false;
        for (auto& [tags, ids] : entry->track_tags()) {
            time_t tag_time = 0;
            table->set (row, 4, tags);
            if (!uri_set) {
                table->set (row, 5, entry->uri());
                uri_set = true;
            }

            std::string t = "";
            for (auto& id : ids) {
                if (!t.empty())
                    t.append(", ");

                t.append(format("@{1}", id.id), colorID);
                tag_time += id.total();
            }

            table->set (row, 6, t);
            table->set (row, 7, Duration (tag_time).formatHours ());
            row = table->addRow ();

            entry_time += tag_time;
        }

        daily_total += entry_time;
    }

    *last_row = row;
    return daily_total;
}

////////////////////////////////////////////////////////////////////////////////
int CmdTimesheet (
        const CLI& cli,
        Rules& rules,
        Database& database)
{
    // Create a filter, and if empty, choose 'today'.
    auto filter = cli.getFilter (Range { Datetime ("today"), Datetime ("tomorrow") });

    // Load the data.
    IntervalFilterAndGroup filtering ({
        std::make_shared <IntervalFilterAllInRange> ( Range { filter.start, filter.end }),
        std::make_shared <IntervalFilterAllWithTags> (filter.tags())
    });

    auto tracked = getTracked (database, rules, filtering);

    if (tracked.empty ())
        return 0;

    // Map tags to colors.
    Color colorID (rules.getBoolean ("color") ? rules.get ("theme.colors.ids") : "");

    Table table;
    _table_init(&table);

    // Each day is rendered separately.
    time_t grand_total = 0;
    Datetime previous;

    auto days_start = filter.is_started() ? filter.start : tracked.front ().start;
    auto days_end   = filter.is_ended()   ? filter.end   : tracked.back ().end;

    if (days_end == 0)
        days_end = Datetime ();

    for (Datetime day = days_start.startOfDay (); day < days_end; ++day)
    {
        time_t daily_total = 0;

        int row = -1;

        std::vector<TimeSheetEntry*> entries = _build_entries(day, tracked);

        if (entries.empty())
            continue;

        row = table.addRow();
        if (day != previous)
        {
            table.set (row, 0, format ("W{1}", day.week ()));
            table.set (row, 1, day.toString ("Y-M-D"));
            table.set (row, 2, day.dayNameShort (day.dayOfWeek ()));
            previous = day;
        }

        daily_total += _table_add_entries(&table, &row, entries, colorID);

        row = table.addRow();

        if (row != -1)
            table.set (row, 8, Duration(daily_total).formatHours());

        grand_total += daily_total;

    }

    // Add the total.
    table.set (table.addRow(), 8, " ", Color ("underline"));
    table.set (table.addRow(), 8, Duration (grand_total).formatHours());

    std::cout << std::endl
              << table.render()
              << std::endl;
    return 0;
}
