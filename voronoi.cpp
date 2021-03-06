#include "voronoi.h"
#include "debug.h"

#include <unordered_map>
#include <cmath>
#include <iterator>

#include "std_ext.h"
#include "geometry.h"

// Types
struct Intersection;
struct BeachCompare;
struct Circle;
typedef std::set<Intersection, BeachCompare> BeachLineT;

// Helper Functions
Circle solveCircle(const Point& p, const Point& q, const Point& r);
Point getIntersection(float sweep_y, const Intersection& inter);
Point getIntersection(float sweep_y, const Point& p, const Point& r, float sign);
Point getIntersection(float sweep_y, const Point& p, double x);
float getSign(const Intersection& intersection);
double sqr(double v);

inline
float perp(const Point& pt, const Point& v0, const Point& v1)
{
    return (pt.x - v1.x) * (v0.y - v1.y) - (pt.y - v1.y) * (v0.x - v1.x);
}


inline
void orderPoints(const Point*& ptA, const Point*& ptB,
        const Point*& ptC)
{
    // ABC -> ABC
    // ACB -> ABC
    // BAC -> ABC
    // BCA -> ACB -> ABC
    // CBA -> CAB -> BAC -> ABC
    // CAB -> BAC -> ABC
    if(ptA > ptB) std::swap(ptA, ptB);
    if(ptB > ptC) std::swap(ptB, ptC);
    if(ptA > ptB) std::swap(ptA, ptB);
}


// Helper Structures
struct Circle
{
    Point center;
    float radius;
};

struct Intersection
{
    Intersection(const Point* pt_left, const Point* pt_right) :
        pt_left(pt_left), pt_right(pt_right) {} ;
    Intersection() : pt_left(nullptr), pt_right(nullptr) {};

    const Point* pt_left;
    const Point* pt_right;
};


struct Boundary
{
    // a boundary connects the midline between two points with a circle point of
    // the original two points and a third point.
    const Point* start_pt_left;
    const Point* start_pt_right;

    const Point* circle_pt;

    bool operator<(const Boundary& rhs)
    {
        return std::min(start_pt_left->y, start_pt_right->y) <
            std::min(rhs.start_pt_left->y, rhs.start_pt_right->y);
    }

    bool operator==(const Boundary& rhs)
    {
        return ((start_pt_left == rhs.start_pt_left &&
                 start_pt_right == rhs.start_pt_right) ||
                (start_pt_left == rhs.start_pt_right &&
                 start_pt_right == rhs.start_pt_left));
    }
};

struct BeachCompare
{
    BeachCompare(float* sweep_y) : sweep_y(sweep_y) {} ;
    float* sweep_y;

    bool operator()(const Intersection& lhs, const Intersection& rhs) const
    {
        // Compares the x index of thwo intersections. In the case where a point
        // is missing (nullptr), there is no intersection (or intersection is at
        // positive or negative infinity, if right or left point is nullptr,
        // respectively)
        std::cerr << "<<<Comparing: ("
            << lhs.pt_left << ", " << lhs.pt_right << ", " << ") to ("
            << rhs.pt_left << ", " << rhs.pt_right << ", " << ")" << std::endl;
        if(lhs.pt_left)
            std::cerr << "<<<Left Point 0: " << *lhs.pt_left << std::endl;
        if(lhs.pt_right)
            std::cerr << "<<<Left Point 1: " << *lhs.pt_right << std::endl;
        if(rhs.pt_left)
            std::cerr << "<<<Right Point 0: " << *rhs.pt_left << std::endl;
        if(rhs.pt_right)
            std::cerr << "<<<Right Point 1: " << *rhs.pt_right << std::endl;
        std::cerr << "<<<Using sweep = " << *sweep_y << std::endl;
        bool lhs_n_infinite = lhs.pt_left == nullptr;
        bool lhs_p_infinite = lhs.pt_right == nullptr;
        bool rhs_n_infinite = rhs.pt_left == nullptr;
        bool rhs_p_infinite = rhs.pt_right == nullptr;

        bool result;
        if((lhs_p_infinite && rhs_n_infinite) ||
                (lhs_p_infinite && rhs_p_infinite) ||
                (lhs_n_infinite && rhs_n_infinite)) {
            // Obviously +infinity !< -infinity and
            // if both inputs have an infinite side in the same direction, then
            // both intersections are dummy boundaries and therefore one can't
            // be less than the other
            result = false;
        } else if(lhs_n_infinite || rhs_p_infinite) {
            // -infinity < rhs => true
            result = true;
        } else if(lhs_p_infinite || rhs_n_infinite) {
            // lhs < -infinity => false
            result = false;
        } else if(lhs.pt_left == rhs.pt_left && lhs.pt_right == rhs.pt_right) {
            // intersection of the exact same to parabolas, by definition this
            // equal and therefore not less
            result = false;
        } else if(lhs.pt_right == rhs.pt_left && lhs.pt_left == rhs.pt_right) {
            // the order of the parabolas determines the sign. So the same two
            // parabolas does not mean the same intersection.
            result = getSign(lhs) < getSign(rhs);
        } else if(lhs.pt_left == lhs.pt_right) {
            // Special case, intersection of two identical points is assumed to
            // be just the x value of the double-point intersection
            assert(rhs.pt_left != rhs.pt_right);
            assert(!(lhs_n_infinite || lhs_p_infinite || rhs_n_infinite ||
                        rhs_p_infinite));
            Point right = getIntersection(*sweep_y, rhs);
            result = lhs.pt_left->x < right.x;
        } else if(rhs.pt_left == rhs.pt_right) {
            // Special case, intersection of two identical points is assumed to
            // be just the x value of the double-point intersection
            assert(lhs.pt_left != lhs.pt_right);
            assert(!(lhs_n_infinite || lhs_p_infinite || rhs_n_infinite ||
                        rhs_p_infinite));
            Point left  = getIntersection(*sweep_y, lhs);
            result = left.x < rhs.pt_left->x;
        } else {
            // get intersection of left two parabolas, and compare x with
            // intersection of right two
            assert(!(lhs_n_infinite || lhs_p_infinite || rhs_n_infinite || rhs_p_infinite));
            std::cerr << "<<<Computing intersections" << std::endl;
            Point left  = getIntersection(*sweep_y, lhs);
            Point right = getIntersection(*sweep_y, rhs);
            std::cerr << "<<<" << (left.x < right.x) << std::endl;
            result = left.x < right.x;
        }

        std::cerr << "<<<" << result << std::endl;
        return result;
    }
};


struct CircleEvent
{
    Intersection left_int;
    Intersection right_int;
    Circle circle;

    bool operator<(const CircleEvent& rhs) const
    {
        return circle.center.y - circle.radius <
            rhs.circle.center.y - rhs.circle.radius;
    }
};


class CircleQueue
{
public:
    bool empty() const
    {
        return m_queue.empty();
    }

    size_t size() const
    {
        return m_queue.size();
    }

    const CircleEvent& front() const
    {
        auto it = m_queue.begin();
        return *it;
    };

    const CircleEvent& back() const
    {
        auto it = m_queue.end();
        it--;
        return *it;
    };

    void pop_back()
    {
        auto it = m_queue.end();
        it--;
        m_queue.erase(it);
    };

    void pop_front()
    {
        auto it = m_queue.begin();
        m_queue.erase(it);
    };

    void insert(double sweep_y, const Intersection& left_int, const Intersection& right_int)
    {
        if(left_int.pt_left == nullptr) return;
        if(right_int.pt_right == nullptr) return;
        if((left_int.pt_left == right_int.pt_left &&
                left_int.pt_right == right_int.pt_right) ||
                (left_int.pt_left == right_int.pt_right &&
                left_int.pt_right == right_int.pt_left)) {
            // if there are only 2 unique points, there should be no event
            return;
        }

        std::cerr << "<<<Inserting Event: ("
            << left_int.pt_left << ", " << left_int.pt_right << ") and ("
            << right_int.pt_left << ", " << right_int.pt_right << ")"
            << std::endl;
        assert(left_int.pt_left);
        assert(left_int.pt_right);
        assert(right_int.pt_left);
        assert(right_int.pt_right);
        std::cerr << "<<<Left Point 0: " << *left_int.pt_left << std::endl;
        std::cerr << "<<<Left Point 1: " << *left_int.pt_right << std::endl;
        std::cerr << "<<<Right Point 0: " << *right_int.pt_left << std::endl;
        std::cerr << "<<<Right Point 1: " << *right_int.pt_right << std::endl;

        //assert(left_int.pt_right == right_int.pt_left);
        auto ptA = left_int.pt_left;
        auto ptB = left_int.pt_right;
        auto ptC = right_int.pt_right;

        CircleEvent evt;
        evt.circle = solveCircle(*ptA, *ptB, *ptC);
        evt.left_int = left_int;
        evt.right_int = right_int;

        // if this is going to happen behind the current sweep, then don't
        // insert. This is effectively a new event behind the beach
        if(evt.circle.center.y - evt.circle.radius > sweep_y)
            return;

        // There can only be 1 circle point for 3 points, ensure that these two
        // intersection actually meet at the circle, othwise don't creat the
        // event
        auto left_int_pt = getIntersection(
                evt.circle.center.y - evt.circle.radius, left_int);
        auto right_int_pt = getIntersection(
                evt.circle.center.y - evt.circle.radius, left_int);
        double d1 = distance2d(left_int_pt, evt.circle.center);
        double d2 = distance2d(right_int_pt, evt.circle.center);
        if(d1 > evt.circle.radius|| d2 > evt.circle.radius) {
            // the intersections should essentially be at the center of the
            // center of the circle. In diverging intersections the distances
            // from the center to intersections will far exceed the radius of
            // the circle.
            return;
        }
        m_queue.insert(evt);
    }


    void erase(const Intersection& left_int, const Intersection& right_int)
    {
        CircleEvent dummy;
        const Point* ptA = left_int.pt_left;
        const Point* ptB = left_int.pt_right;
        const Point* ptC = right_int.pt_right;
        assert(ptB != nullptr);

        // no event to erase since one of the "intersections" is the null
        // intersection
        if(ptA == nullptr || ptC == nullptr)
            return;

        dummy.circle = solveCircle(*ptA, *ptB, *ptC);
        float end_y = dummy.circle.center.y - dummy.circle.radius;

        for(auto it = m_queue.lower_bound(dummy); it != m_queue.end() &&
                !(it->circle.center.y - it->circle.radius < end_y); ++it) {
            if(left_int.pt_left == it->left_int.pt_left &&
                    left_int.pt_right == it->left_int.pt_right &&
                    right_int.pt_left == it->right_int.pt_left &&
                    right_int.pt_right == it->right_int.pt_right) {
                it = m_queue.erase(it);
                break;
            }
        }
    }

    typedef std::set<CircleEvent>::iterator iterator;
    typedef std::set<CircleEvent>::const_iterator const_iterator;

    iterator begin() { return m_queue.begin(); };
    iterator end() { return m_queue.end(); };
    const_iterator cbegin() const { return m_queue.cbegin(); };
    const_iterator cend() const { return m_queue.cend(); };
private:

    std::set<CircleEvent> m_queue;
};


// State Holder (beach line and output voronoi diagram)
class Voronoi::Implementation
{
    public:
    Implementation() : m_beach_compare(&sweep_y), m_beach(m_beach_compare),
        m_min_x(std::numeric_limits<double>::infinity()),
        m_max_x(-std::numeric_limits<double>::infinity()),
        m_min_y(std::numeric_limits<double>::infinity()),
        m_max_y(-std::numeric_limits<double>::infinity())
    {
    }

    void compute(const std::vector<Point>& points);

    private:
    void processPoint(const Point& pt);
    void processEvent(const CircleEvent& event);

    Node::Ptr getNode(const Point* ptA, const Point* ptB);
    Node::Ptr getNode(const Point* ptA, const Point* ptB, const Point* ptC);

    std::shared_ptr<Edge> addEdge(
            Node::Ptr nodeA,
            Node::Ptr nodeB);

    void addTriplet(Node::Ptr center,
            Node::Ptr nodeA,
            Node::Ptr nodeB,
            Node::Ptr nodeC);

    float sweep_y;
    BeachCompare m_beach_compare;
    BeachLineT m_beach;
    CircleQueue m_events;

    double m_min_x, m_max_x, m_min_y, m_max_y;

    std::unordered_map<std::tuple<const Point*, const Point*, const Point*>,
        Node::Ptr> m_nodes;
    std::vector<Edge::Ptr> m_edges;
    const std::vector<Point>* m_points;

	friend Voronoi;
};

/**
 * Functions
 */

bool points_match(std::tuple<const Point*, const Point*, const Point*> lhs,
        std::tuple<const Point*, const Point*, const Point*> rhs)
{
    return (std::get<0>(lhs) == std::get<0>(rhs) &&
            std::get<1>(lhs) == std::get<1>(rhs) &&
            std::get<2>(lhs) == std::get<2>(rhs)) ||
           (std::get<0>(lhs) == std::get<0>(rhs) &&
            std::get<1>(lhs) == std::get<2>(rhs) &&
            std::get<2>(lhs) == std::get<1>(rhs)) ||
           (std::get<0>(lhs) == std::get<1>(rhs) &&
            std::get<1>(lhs) == std::get<2>(rhs) &&
            std::get<2>(lhs) == std::get<0>(rhs)) ||
           (std::get<0>(lhs) == std::get<1>(rhs) &&
            std::get<1>(lhs) == std::get<0>(rhs) &&
            std::get<2>(lhs) == std::get<2>(rhs)) ||
           (std::get<0>(lhs) == std::get<2>(rhs) &&
            std::get<1>(lhs) == std::get<0>(rhs) &&
            std::get<2>(lhs) == std::get<1>(rhs)) ||
           (std::get<0>(lhs) == std::get<2>(rhs) &&
            std::get<1>(lhs) == std::get<1>(rhs) &&
            std::get<2>(lhs) == std::get<0>(rhs));
}

/**
 *  Find the intersection of two parabolas created from points and a sweep line
 *  (directrix)
 *
 *  Note: order is important
 *
 *  When a parabola is initially inserted, it is essentially a line:
 *
 *  \  A  /
 *   *   /
 *   |\ /
 *   | v
 *   |
 * --B---------
 *
 *  \  A  /
 *   \   *
 *    \ /|
 *     v |
 *       |
 * ------B------
 *
 *  then expands to a parabola
 *
 *  *  A  /
 *  |\   /
 *  | * /
 *  \ /v
 *   v
 * --B---------
 *
 *  \  A  *
 *   \   //
 *    \ * |
 *     v| |
 *       v
 * ------B------
 *
 * The beach line is made up of segments on the frontier of these two (and
 * other) parabolas. We define the beach line by storing the series of
 * intersections:
 *
 * -inf:A, A:B, B:A, A:inf
 *
 * where inf means there is no intersection and the beach line continues forever
 * along the curve. intersections are shown with *'s in the figures.
 *
 * The possible intersections defined by left_parab and right_parab are:
 * A:B, B:A. where the sign of the radical for parabola B is -1 for A:B and +1
 * for B:A. In other words, the sign of the radical determines the side of B
 * that we are on
 *
 * @param sweep_y Position of sweep line
 * @param left parab Beach line parabola that on the left side of the
 * intersection. NOTE that this is different from the x-coordinate of the focal
 * points. Left parab could have a focal point x > OR < than the focal point of
 * right parab.
 * @param right_parab Becah line parabola that is on the right side of the
 * intersection. NOTE that this is different from the x-coordinate of the focal
 * points. Left parab could have a focal point x > OR < than the focal point of
 * right parab.

 */
float getSign(const Intersection& intersection)
{
    assert(intersection.pt_left && intersection.pt_right);

    const Point& left_parab = *intersection.pt_left;
    const Point& right_parab = *intersection.pt_right;
    if(left_parab.y <= right_parab.y) {
        // A = right_parab
        // B = left_parab
        // Transitioning form B to A (B:A)
        return 1;
    } else {
        // A = left_parab
        // B = right_parab
        // Transitioning from A to B (A:B)
        return -1;
    }
}

Point getIntersection(float sweep_y, const Intersection& inter)
{
    assert(inter.pt_left != nullptr);
    assert(inter.pt_right != nullptr);
    return getIntersection(sweep_y, *inter.pt_left, *inter.pt_right, getSign(inter));
}

Point getIntersection(float sweep_y, const Point& p, const Point& r, float sign)
{
    // Given any point q, its distance from sweep is:
    //
    // (q.y - y_s)
    //
    // Its distance from an event p is:
    //
    // sqrt((q.x - p.x)^2 + (q.y - p.y)^2)
    //
    // the parabola equation is then:
    //
    // (q.y - y_s)^2 = (q.x - p.x)^2 + (q.y - p.y)^2
    //
    // For two event points, r and p, we can solve q.x -- the intersection
    // location must satisfy:
    //
    // sqrt( sqr(q_x - p_x) + sqr(q_y - p_y) ) ==
    // sqrt( sqr(q_x - r_x) + sqr(q_y - r_y) )
    //
    // and
    //
    // sqrt( sqr(q_x - r_x) + sqr(q_y - r_y) ) == q_y - sweep_y
    std::cerr << "<<<<Intersection of:\n<<<<" << p << "\n<<<<" << r << "\n<<<<" <<
        sweep_y << std::endl;

    // Solve for x first
    float y_s = sweep_y;
    Point q;
    if(std::abs(p.y - sweep_y) < 0.0000001) {
        // parabola around p has no width, just select point on parabola r at
        // p.x
        std::cerr << "<<<<p_y == sweep_y" << std::endl;
        q.x = p.x;
        q.y = 0.5*( sqr(q.x) - 2*q.x*r.x + sqr(r.x) + sqr(r.y) - sqr(y_s))/(r.y - y_s);
    } else if(std::abs(r.y - sweep_y) < 0.0000001) {
        // parabola around r has no width, just select point on parabola q at
        // r.x
        std::cerr << "<<<<r_y == sweep_y" << std::endl;
        q.x = r.x;
        q.y = 0.5*(p.x*p.x + p.y*p.y - 2*p.x*q.x + q.x*q.x - y_s*y_s)/(p.y - y_s);
    } else {
        if(std::abs(p.y - r.y) > 0.0000001) {
            float term1 = (p.y*r.x - p.x*r.y + (p.x - r.x)*y_s) / (p.y - r.y);
            float rad =
                sqrt(p.x*p.x + p.y*p.y - 2*p.x*r.x + r.x*r.x - 2*p.y*r.y + r.y*r.y)*
                sqrt(p.y - y_s)*sqrt(r.y - y_s)/(p.y - r.y);

            std::cerr << "<<<<"
                << sqrt(p.x*p.x + p.y*p.y - 2*p.x*r.x + r.x*r.x - 2*p.y*r.y + r.y*r.y)
                << ", " << sqrt( p.y - y_s) << ", " << sqrt(r.y - y_s)/(p.y - r.y) << std::endl;;
            std::cerr << "<<<<" << term1 << " + " << sign << " * " << rad << std::endl;
            // choose +- radical to be between
            q.x = term1 + sign*std::abs(rad);

            // use one of the parabolas to find y
            q.y = 0.5*(p.x*p.x + p.y*p.y - 2*p.x*q.x + q.x*q.x - y_s*y_s)/(p.y - y_s);
        } else {
            // special case: x-coord is exactly in-between
            q.x = (p.x + r.x)*0.5;
            q.y = 0.5*(sqr(p.x) + sqr(p.y) - 2*p.x*q.x + sqr(q.x) - sqr(y_s))/
                (p.y - y_s);
        }
    }

    assert(!std::isinf(q.x));
    assert(!std::isinf(q.y));
    assert(!std::isnan(q.x));
    assert(!std::isnan(q.y));
    std::cerr << "<<<<Solution: " << q << std::endl;
    std::cerr << "<<<<Sweep line distance0: " << (q.y - sweep_y) << "\n"
        << "<<<<Solution distance0: "
        << std::sqrt( sqr(p.x - q.x ) + sqr( p.y - q.y )) << std::endl;
    std::cerr << "<<<<Sweep line distance1: " << (q.y - sweep_y) << "\n"
        << "<<<<Solution distance1: "
        << std::sqrt( sqr(r.x - q.x ) + sqr( r.y - q.y )) << std::endl;
    return q;
}

inline
double sqr(double v) {
    return v*v;
}

Circle solveCircle(const Point& p, const Point& q, const Point& r)
{
    // find minimal point
    Circle circle;
    circle.center.x =
        0.5*(p.y*sqr(q.x) + p.y*sqr(q.y) - (p.y - q.y)*sqr(r.x) -
                (p.y - q.y)*sqr(r.y) - (sqr(p.x) + sqr(p.y))*q.y +
                (sqr(p.x) + sqr(p.y) - sqr(q.x) - sqr(q.y))*r.y) /
        (p.y*q.x - p.x*q.y - (p.y - q.y)*r.x + (p.x - q.x)*r.y);

    circle.center.y =
        -0.5*(p.x*sqr(q.x) + p.x*sqr(q.y) - (p.x - q.x)*sqr(r.x) -
                (p.x - q.x)*sqr(r.y) - (sqr(p.x) + sqr(p.y))*q.x +
                (sqr(p.x) + sqr(p.y) - sqr(q.x) - sqr(q.y))*r.x) /
        (p.y*q.x - p.x*q.y - (p.y - q.y)*r.x + (p.x - q.x)*r.y);

    circle.radius = sqrt(sqr(p.x - circle.center.x) + sqr(p.y - circle.center.y));
    return circle;
};

// Voronoi::implementation Implementation
void Voronoi::Implementation::processEvent(const CircleEvent& event)
{
    assert(event.left_int.pt_right == event.right_int.pt_left);
    assert(event.left_int.pt_right == event.right_int.pt_left);
    std::cerr << "--------\nProcessing Event at "
        << (event.circle.center.y - event.circle.radius)
        << " for: [" << event.left_int.pt_left << " -- "
        << event.left_int.pt_right << "], [" << event.right_int.pt_left << " -- "
        << event.right_int.pt_right << "]\n";

    // This essentially locks in the results of a single point (the middle part
    // of the two intersections, that means we must remove all events related to
    // this point


    // Suppose that intersections A and B meet (when the middle peak is beat out
    // by the side peaks):
    // l |   |     | r
    //   \  A/ |   |
    //    \ / v\B  /
    //     v    \ /
    //           v

    // |     |
    // |     |      |
    // \    C/      |
    //  \   / \     /
    //   \ /   \   /
    //    v     \ /
    //           v
    //
    // First we will remove the intersections A and B from the beach line
    //
    // Now if there are other intersections to the left of A, there would be an
    // event associated with A and that (l), same for an event to the right of
    // B (r). Those events can no longer occur because the intersections A (of
    // the left and center peak) and B (of the center and right peaks) will no
    // longer exist when the center peak has been hidden. Therefore we must
    // remove those events.
    //
    // We then have a new intersection on the beach line (C) which can have
    // events with l and r (when they exist)

    // find intersections to the left and right on the beach line, so we can
    // create a new event for when they meet
    {
    for(auto it1 = m_beach.begin(); it1 != m_beach.end(); ++it1) {
        auto it2 = it1;
        it2++;
        if(it2 != m_beach.end() && !m_beach_compare(*it1, *it2)) {
            std::cerr << it1->pt_left << ", " << it1->pt_right
                << " comes before " << it2->pt_left << ", " << it2->pt_right
                << "but it is not less!" << std::endl;
            throw -1;
        }
    }
    }

    bool success;
    BeachLineT::iterator it_new;
    std::cerr << "Looking up event location" << std::endl;
    auto it = m_beach.find(event.left_int);
    assert(it != m_beach.begin());
    assert(it != m_beach.end());

    std::cerr << "Left Int: [" << *(*it).pt_left << " -- " << *(*it).pt_right << std::endl;
    it--;
    auto left_neighbor = *it;
    it++;
    auto left_it = it;
    assert(left_it->pt_right == event.left_int.pt_right);
    assert(left_it->pt_left == event.left_int.pt_left);
    it++;
    auto right_it = it;
    assert(right_it->pt_right == event.right_int.pt_right);
    assert(right_it->pt_left == event.right_int.pt_left);
    std::cerr << "Right Int: [" << *(*it).pt_left << " -- " << *(*it).pt_right << std::endl;
    it++;
    auto right_neighbor = *it;
    assert(left_neighbor.pt_right == event.left_int.pt_left);
    assert(right_neighbor.pt_left == event.right_int.pt_right);

    // Find the 3 unique points so that we can create the necessary boundary
    // lines
    const Point* ptA = event.left_int.pt_left;
    const Point* ptB = event.left_int.pt_right;
    const Point* ptC = event.right_int.pt_right;

    // erase any other meetings with these two events
    m_events.erase(left_neighbor, event.left_int);
    m_events.erase(event.right_int, right_neighbor);

    // delete arc (i.e. erase both intersections related to the current event)
    std::cerr << "Erasing from beach" << std::endl;
    m_beach.erase(left_it);
    m_beach.erase(right_it);

    // Update sweep location so that our beach inserts go in the correct
    // location. Note we do this after the beach erase because technically at
    // this event the left and right intersections meet so there might be a
    // little strangeness with the ordering at sweep_y. Therefore just erase the
    // points first (above)
    *m_beach_compare.sweep_y = event.circle.center.y - event.circle.radius;

    // create new intersection of the outtermost arcs (left point of left
    // intersection and right point of right intersection)
    std::cerr << "Creating new beach point" << std::endl;
    std::tie(it_new, success) = m_beach.emplace(event.left_int.pt_left,
            event.right_int.pt_right);
    assert(success);

    // create new event(s) for the meeting of the new intersection and its
    // neighors, excepting the cases where 1) there is no neighboring
    // intersection because the neighbor is a special endpoint (nullptr for one
    // of its points) or 2) the neighboring intersection and new intersection
    // have the same three points that we just processed
    if(left_neighbor.pt_left != nullptr) {
        // Make sure that we aren't creating a new event for the points we just
        // processed
        auto event_points = std::make_tuple(left_neighbor.pt_left,
                it_new->pt_left, it_new->pt_right);
        if(!points_match(event_points, std::make_tuple(ptA, ptB, ptC)))
            m_events.insert(*m_beach_compare.sweep_y, left_neighbor, *it_new);
    }
    if(right_neighbor.pt_right != nullptr) {
        // Make sure that we aren't creating a new event for the points we just
        // processed
        auto event_points = std::make_tuple(it_new->pt_left, it_new->pt_right,
                right_neighbor.pt_right);
        if(!points_match(event_points, std::make_tuple(ptA, ptB, ptC)))
            m_events.insert(*m_beach_compare.sweep_y, *it_new, right_neighbor);
    }

//    Line line0{*event.left_int.pt_left, *event.left_int.pt_right};
//    Line line1{*event.right_int.pt_left, *event.right_int.pt_right};
//    lines.push_back(line0);
//    lines.push_back(line1);
//
//    // finish off boundaries related to the middle point
//    auto itb = m_bounds.find(left_neighbor.pt_left, left_neighbor.pt_right);
//    assert(itb != m_bounds.end());
//    itb->circle_pt = right_neighbor.pt_right;
//
//    itb = m_bounds.find(right_neighbor.pt_left, right_neighbor.pt_right);
//    assert(itb != m_bounds.end());
//    itb->circle_pt = left_neighbor.pt_left;
        for(const auto& tup_node: m_nodes) {
            assert(tup_node.second != nullptr);
            std::cerr << tup_node.second->x << ", "
                << tup_node.second->y << std::endl;
        }

    // The new center point connects to bisectors of each of the individual
    // pairs of points, these are rays from the center of the event circle to
    // each of the bisectors. Note that the first two points define the line
    // beginning, so all 3 possible pairs of the 3 points must show up
    Node::Ptr nodeCenter = getNode(ptA, ptB, ptC);
    Node::Ptr nodeAB = getNode(ptA, ptB);
    Node::Ptr nodeBC = getNode(ptB, ptC);
    Node::Ptr nodeCA = getNode(ptA, ptC);
    assert(nodeCenter != nullptr);
    assert(nodeAB != nullptr);
    assert(nodeBC != nullptr);
    assert(nodeCA != nullptr);

        for(const auto& tup_node: m_nodes) {
            assert(tup_node.second != nullptr);
            std::cerr << tup_node.second->x << ", "
                << tup_node.second->y << std::endl;
        }
    float distAB = perp(event.circle.center, *ptA, *ptB);
    float distBC = perp(event.circle.center, *ptB, *ptC);
    float distCA = perp(event.circle.center, *ptC, *ptA);
    if((distAB <= 0 && distBC <= 0 && distCA <= 0) ||
            (distAB >= 0 && distBC >= 0 && distCA >= 0)) {
        // point inside triangle
        addTriplet(nodeCenter, nodeAB, nodeBC, nodeCA);
    } else {
        // Whichever ever side of the triangle had the opposite sign as the
        // other two, is the one that we need to connect with
        std::cerr << "center = ["<< event.circle.center << "]"
            << "\n\ttriangle = [" << *ptA << ";"
            << "\n\t" << *ptB << ";"
            << "\n\t" << *ptC << "]"
            << std::endl;
        if((distBC <= 0 && (distCA >= 0 && distAB >=0)) ||
                (distBC >= 0 && (distCA <= 0 && distAB <=0))) {
            // distBC is the odd man out, move ptC into ptA so that A, B are
            // nearest
            addTriplet(nodeBC, nodeCenter, nodeCA, nodeAB);
        } else if((distCA <= 0 && (distAB >= 0 && distBC >=0)) ||
                (distCA >= 0 && (distAB <= 0 && distBC <=0))) {
            // distCA is the odd man out, swap nodeAB and nodeCA
            addTriplet(nodeCA, nodeCenter, nodeAB, nodeBC);
        } else {
            addTriplet(nodeAB, nodeCenter, nodeBC, nodeCA);
        }

    }
}


void Voronoi::Implementation::processPoint(const Point& pt)
{
    std::cerr << "<----------------------" << std::endl;
    std::cerr << "<Processing point: " << pt << std::endl;

    // Update sweep location in beach line so that insertion takes place at the
    // right location

    *m_beach_compare.sweep_y = pt.y;

    // insert two new intersections in between existing intersections
    Intersection dummy{&pt, &pt};
    bool success;
    BeachLineT::iterator it1, it2, it_new;
    const Point* ptA = nullptr;
    const Point* ptB = nullptr;
    const Point* ptC = nullptr;
    const Point* ptD = nullptr;
    if(m_beach.empty()) {
        std::cerr << "<<<Beach empty, inserting special" << std::endl;
        // add null intersection
        // no intersections to erase
        m_beach.emplace(nullptr, &pt);
        m_beach.emplace(&pt, nullptr);
    } else {
        // In between two previous intersections, on the parabolar for the
        // shared point
        //
        //  iterator:      it1       it2
        //  struct:        pt_left pt_right   pt_left pt_right
        //  points:         A   B     B   C
        //  new inter:          B  D  B
        // intersection >= so take the first point
        std::cerr << "<<Finding beach location" << std::endl;
        it1 = m_beach.lower_bound(dummy);
        std::cerr << "<<Lower bound: (" << it1->pt_left << " -- "
            << it1->pt_right << ")" << std::endl;
        if(it1->pt_left) {
            std::cerr << "<<pt_left: " << *it1->pt_left << std::endl;
        }
        if(it1->pt_right) {
            std::cerr << "<<pt_right: " << *it1->pt_right << std::endl;
        }
        std::cerr << "<<Done" << std::endl;
        it2 = it1; it1--;
        ptB = it1->pt_right;
        ptD = &pt;

        std::cerr << "B: " << ptB << std::endl
            << "D: " << ptD << std::endl;
        // Insert new intersection into beach, then create an event for the old
        // left and the new intersection point
        std::cerr << "Inserting " << ptB << ", " << ptD << " into beach" << std::endl;
        std::tie(it_new, success) = m_beach.emplace(ptB, ptD);
        assert(success);
        if(it1->pt_left != nullptr)
            m_events.insert(*m_beach_compare.sweep_y, *it1, *it_new);

        // Insert new intersection int beach, then create a new event for the
        // old upper intersection and the new one
        std::cerr << "Inserting " << ptD << ", " << ptB << " into beach" << std::endl;
        std::tie(it_new, success) = m_beach.emplace(ptD, ptB);
        assert(success);
        if(it2->pt_right != nullptr)
            m_events.insert(*m_beach_compare.sweep_y, *it_new, *it2);

        // Erase the event that involved the meeting of our previous left and
        // right intersections (since we got in the middle)
        if(it1->pt_left != nullptr && it2->pt_right != nullptr) {
            m_events.erase(*it1, *it2);
        }
    }


    std::cerr << "<......................" << std::endl;
}

void Voronoi::Implementation::compute(const std::vector<Point>& points)
{
    m_points = &points;
    for(const auto& pt : points) {
        m_min_x = std::min<double>(pt.x, m_min_x);
        m_max_x = std::max<double>(pt.x, m_max_x);
        m_min_y = std::min<double>(pt.y, m_min_y);
        m_max_y = std::max<double>(pt.y, m_max_y);
    }

    std::cerr << "Sorting points" << std::endl;
    // Sort by decreasing y
    std::vector<size_t> ordered(points.size());
    for(size_t ii = 0; ii < points.size(); ii++) ordered[ii] = ii;
    std::sort(ordered.begin(), ordered.end(),
            [&](size_t ii, size_t jj) { return points[ii].y > points[jj].y; });

    // stop when circle event's centers are after this
    double last_y = points[ordered.back()].y;

    std::cerr << "Ordered points: " << std::endl;
    for(size_t ii : ordered) {
        std::cerr << points[ii] << std::endl;
    }
    std::cerr << std::endl;

    // Travel downward so at each step take
    size_t ii = 0;
    double prev_sweep = NAN;
    double sweep = NAN;
    while(!m_events.empty() || ii < ordered.size()) {
        std::cerr << "Remaining Points: " << (ordered.size() - ii) << std::endl;
        std::cerr << "Remaining Events: " << m_events.size() << std::endl;

        if(m_events.empty()) {
            std::cerr << "Events Empty, processing next point" << std::endl;
            sweep = points[ordered[ii]].y;
            draw_state(m_beach, m_events, prev_sweep, sweep);
            prev_sweep = sweep;
            processPoint(points[ordered[ii]]);
            ii++;
        } else if(ii == ordered.size()) {
            std::cerr << "Points Done, processing next event" << std::endl;
            auto evt = m_events.back(); // greater y's first (decreasing y)
            std::cerr << evt.circle.center.y << std::endl;
            sweep = evt.circle.center.y - evt.circle.radius;
            draw_state(m_beach, m_events, prev_sweep, sweep);
            prev_sweep = sweep;
            m_events.pop_back();
            processEvent(evt);
        } else {
            auto evt = m_events.back(); // greater y's first (decreasing y)
            std::cerr << "Next point: " << points[ordered[ii]].y
                << ", Next Event: " << evt.circle.center.y - evt.circle.radius
                << std::endl;
            if(points[ordered[ii]].y > evt.circle.center.y - evt.circle.radius) {
                sweep = points[ordered[ii]].y;
                draw_state(m_beach, m_events, prev_sweep, sweep);
                prev_sweep = sweep;
                processPoint(points[ordered[ii]]);
                ii++;
            } else {
                sweep = evt.circle.center.y - evt.circle.radius;
                draw_state(m_beach, m_events, prev_sweep, sweep);
                    prev_sweep = sweep;
                m_events.pop_back();
                processEvent(evt);
            }
        }

        std::cerr << "Final Beach: " << std::endl;
        for(const auto& inter: m_beach) {
            std::cerr << "(" << inter.pt_left << ", " << inter.pt_right << ")";
            if(inter.pt_left) std::cerr << "Point 0: " << *inter.pt_left << " ";
            if(inter.pt_right) std::cerr << "Point 1: " << *inter.pt_right << " ";
            std::cerr << std::endl;
        }

        std::cerr << "Final Events: " << std::endl;
        for(const auto& evt: m_events) {
            std::cerr << "at " << (evt.circle.center.y - evt.circle.radius)
                << "( "
                << evt.left_int.pt_left << ", "
                << evt.left_int.pt_right << ")"
                << " -- "
                << evt.right_int.pt_left << ", ("
                << evt.right_int.pt_right << ")"
                << std::endl;
        }

        for(const auto& tup_node: m_nodes) {
            assert(tup_node.second != nullptr);
            std::cerr << tup_node.second->x << ", "
                << tup_node.second->y << std::endl;
        }
    }

    //return voronoi;
}


Voronoi::Node::Ptr Voronoi::Implementation::getNode(
        const Point* ptA, const Point* ptB, const Point* ptC)
{
    orderPoints(ptA, ptB, ptC);
    auto result = m_nodes.emplace(std::make_tuple(ptA, ptB, ptC), nullptr);

    // if node exists, just return it
    if(!result.second) {
        assert(result.first->second != nullptr);
        return result.first->second;
    }

    // create and fill out the new node
    auto circle = solveCircle(*ptA, *ptB, *ptC);

    // need to construct a new node and add its parents and location
    auto new_node = std::make_shared<Node>();
    result.first->second = new_node;

    // Add parents
    new_node->parents.insert((ptA - m_points->data()) / sizeof(Point));
    new_node->parents.insert((ptB - m_points->data()) / sizeof(Point));
    new_node->parents.insert((ptC - m_points->data()) / sizeof(Point));

    // Add position
    new_node->x = circle.center.x;
    new_node->y = circle.center.y;

    return new_node;
}

Voronoi::Node::Ptr Voronoi::Implementation::getNode(
        const Point* ptA, const Point* ptB)
{
    if(ptA > ptB) std::swap(ptA, ptB);
    auto result = m_nodes.emplace(std::make_tuple(ptA, ptB, nullptr), nullptr);

    // if node exists, just return it
    if(!result.second) {
        assert(result.first->second != nullptr);
        return result.first->second;
    }

    // need to construct a new node and add its parents and location
    auto new_node = std::make_shared<Node>();
    result.first->second = new_node;

    // Add parents
    new_node->parents.insert((ptA - m_points->data()) / sizeof(Point));
    new_node->parents.insert((ptB - m_points->data()) / sizeof(Point));

    // Add position
    new_node->x = (ptA->x + ptB->x)*0.5;
    new_node->y = (ptA->y + ptB->y)*0.5;

    return new_node;
}

Voronoi::Edge::Ptr Voronoi::Implementation::addEdge(
        Node::Ptr nodeA,
        Node::Ptr nodeB)
{
    // the edges parents are the parents in common between the two input nodes
    std::set<size_t> common;
    std::set_intersection(
            nodeA->parents.begin(), nodeA->parents.end(),
            nodeB->parents.begin(), nodeB->parents.end(),
            std::inserter(common, common.begin()));

    auto out = std::make_shared<Edge>();
    out->parents = common;
    out->nodes[0] = nodeA;
    out->nodes[1] = nodeB;
    std::cerr << "adding edge!" << std::endl;
    std::cerr << "edge = [" << nodeA->x << ", " << nodeA->y << ";"
        << nodeB->x << ", " << nodeB->y << "]" << std::endl;
    m_edges.push_back(out);
    return out;
}

void Voronoi::Implementation::addTriplet(Node::Ptr center, Node::Ptr nodeA,
        Node::Ptr nodeB, Node::Ptr nodeC)
{
    // edges
    std::shared_ptr<Edge> edgeA = addEdge(nodeA, center);
    std::shared_ptr<Edge> edgeB = addEdge(nodeB, center);
    std::shared_ptr<Edge> edgeC = addEdge(nodeC, center);

    nodeA->edges.insert(edgeA);
    nodeB->edges.insert(edgeB);
    nodeC->edges.insert(edgeC);

    center->edges.insert(edgeA);
    center->edges.insert(edgeB);
    center->edges.insert(edgeC);

    // neighbors
    nodeA->neighbors.insert(center);
    nodeB->neighbors.insert(center);
    nodeC->neighbors.insert(center);

    center->neighbors.insert(nodeA);
    center->neighbors.insert(nodeB);
    center->neighbors.insert(nodeC);
}

Voronoi::Voronoi(const std::vector<Point>& points)
{
    using std::unordered_map;
    using std::tuple;
    using std::make_tuple;

    Implementation impl;
    impl.compute(points);

    std::cerr << "Done with computation" << std::endl;
    m_nodes.clear();
    m_nodes.reserve(impl.m_nodes.size());
    for(const auto& tup_node: impl.m_nodes) {
        assert(tup_node.second != nullptr);
        m_nodes.push_back(tup_node.second);
        std::cerr << m_nodes.back()->x << ", " << m_nodes.back()->y << std::endl;
    }
    m_edges = impl.m_edges;

    // add edge's neighbors by copying the neighbors of
    for(Edge::Ptr edge : m_edges) {
        std::cerr << edge->nodes[0]->x << ", " << edge->nodes[0]->y << " -- "
            << edge->nodes[1]->x << ", " << edge->nodes[1]->y << std::endl;
        for(const auto& neighbor : edge->nodes[0]->edges) {
            if(neighbor != edge)
                edge->neighbors.insert(neighbor);
        }
        for(const auto& neighbor : edge->nodes[1]->edges) {
            if(neighbor != edge)
                edge->neighbors.insert(neighbor);
        }
    }
}
