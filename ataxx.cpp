#include <algorithm>
#include <cstring>
#include <ctime>
#include <iostream>

#define MAX1 100
#define MAX0 50
#define MIN1 -100
#define MIN0 -50
#define limitT 0.9
#define detectN 3
#define initDepth 4

using namespace std;

static uint8_t mask[4] = {0x15, 0, 0, 0x15};
static char code[4] = {'#', '#', 'b', 'w'};
static int8_t delta[24][2] = {
        {1, 1},  {0, 1},   {-1, 1},  {-1, 0},  {-1, -1}, {0, -1}, {1, -1}, {1, 0},
        {2, 0},  {2, 1},   {2, 2},   {1, 2},   {0, 2},   {-1, 2}, {-2, 2}, {-2, 1},
        {-2, 0}, {-2, -1}, {-2, -2}, {-1, -2}, {0, -2},  {1, -2}, {2, -2}, {2, -1}};

static bool timeUp = false, itSTOP = false;
static clock_t clk0;

struct moveStep {
    int8_t startX, startY, endX, endY;
};

istream &operator>>(istream &is, moveStep &m) {
    int startX, startY, endX, endY;
    is >> startX >> startY >> endX >> endY;
    m.startX = startX;
    m.startY = startY;
    m.endX = endX;
    m.endY = endY;
    return is;
}

ostream &operator<<(ostream &os, moveStep &m) {
    os << int(m.startX) << ' ';
    os << int(m.startY) << ' ';
    os << int(m.endX) << ' ';
    os << int(m.endY);
    return os;
}

class storedNode;
class storedTree;

class chessBoard {
    friend class storedNode;
    friend class storedTree;
    // 0/1-empty 2-black 3-white
    uint16_t info[7];
    int8_t currColor, storedVal;

 public:
    chessBoard() {}
    chessBoard(istream &is) {
        memset(info, 0, 7 * sizeof(uint16_t));
        info[0] = 0xC008;  // |B|W|
        info[6] = 0x800C;  // |W|B|
        currColor = 2;
        int turnID;
        is >> turnID;
        moveStep tmp;
        for (int i = 1; i != turnID; ++i) {
            // enemy's turn to procstep
            is >> tmp;
            if (tmp.endX >= 0) procStep(tmp);
            // our turn to procstep
            is >> tmp;
            procStep(tmp);
        }
        is >> tmp;
        if (tmp.endX >= 0) procStep(tmp);
    }
    chessBoard(const chessBoard &B) {
        memcpy(info, B.info, 7 * sizeof(uint16_t));
        currColor = B.currColor;
        storedVal = B.storedVal;
    }
    bool inMap(int8_t x, int8_t y) {
        return x >= 0 && x <= 6 && y >= 0 && y <= 6;
    }
    int8_t pos(int8_t x, int8_t y) { return (info[x] >> ((y + 1) << 1)) & 3; }
    bool avail(int8_t x, int8_t y) {
        return ((info[x] >> ((y + 1) << 1)) & 3) == currColor;
    }
    bool empty(int8_t x, int8_t y) { return !((info[x] >> ((y + 1) << 1)) & 2); }
    void procStep(const moveStep &m) {
        int8_t startX = m.startX, startY = m.startY, endX = m.endX, endY = m.endY;
        if (abs(endX - startX) == 2 || abs(endY - startY) == 2)
            info[startX] ^= (2 << ((startY + 1) << 1));
        info[endX] &= ~(3 << ((endY + 1) << 1));
        info[endX] |= (currColor << ((endY + 1) << 1));
        for (int8_t i = -1; i <= 1; ++i)
            if (endX + i >= 0 && endX + i <= 6) {
                info[endX + i] &= ~(mask[0] << (endY << 1));
                info[endX + i] |= (mask[currColor] << (endY << 1));
            }
        currColor = 5 - currColor;
    }
    void val() {
        int8_t sum[4] = {0};
        for (uint8_t i = 0; i != 7; ++i) {
            uint16_t line = info[i];
            for (uint8_t j = 0; j != 7; ++j) ++sum[(line >> ((j + 1) << 1)) & 3];
        }
        storedVal = sum[currColor] - sum[5 - currColor];
    }
    void end(int8_t level) {
        int8_t sum[4] = {0};
        for (uint8_t i = 0; i != 7; ++i) {
            uint16_t line = info[i];
            for (uint8_t j = 0; j != 7; ++j) ++sum[(line >> ((j + 1) << 1)) & 3];
        }
        storedVal = sum[currColor] <= 24 ? MIN1 + level : MAX1 + level;
    }
    void print() {
        for (int8_t i = 0; i != 7; ++i) {
            for (int8_t j = 0; j != 7; ++j) cout << code[pos(i, j)];
            cout << endl;
        }
        cout << int(storedVal) << endl;
    }
};

class storedNode : public chessBoard {
    friend class storedTree;
    moveStep from;
    uint16_t num;
    storedNode *chld;

 public:
    storedNode() : chessBoard(), num(0xFFFF), chld(NULL) {}
    storedNode(const chessBoard &B) : chessBoard(B) {
        num = 0xFFFF;
        chld = NULL;
    }
    void deleteNode() {
        if (num == 0xFFFF) return;
        for (uint16_t i = 0; i != num; ++i) chld[i].deleteNode();
        if (!chld) delete[] chld;
    }
    void calcAvail(bool dsort) {
        if (!(uint16_t(~num))) {
            moveStep av[600];
            num = 0;
            for (int8_t x0 = 0; x0 != 7; ++x0)
                for (int8_t y0 = 0; y0 != 7; ++y0)
                    if (empty(x0, y0)) {
                        uint8_t i;
                        for (i = 0; i != 8; ++i) {
                            int8_t x1 = x0 + delta[i][0], y1 = y0 + delta[i][1];
                            if (!inMap(x1, y1)) continue;
                            if (!avail(x1, y1)) continue;
                            moveStep tmp = {x1, y1, x0, y0};
                            av[num++] = tmp;
                            break;
                        }
                        for (i = 8; i != 24; ++i) {
                            int8_t x1 = x0 + delta[i][0], y1 = y0 + delta[i][1];
                            if (!inMap(x1, y1)) continue;
                            if (!avail(x1, y1)) continue;
                            moveStep tmp = {x1, y1, x0, y0};
                            av[num++] = tmp;
                        }
                    }
            chld = new storedNode[num];
            for (uint16_t i = 0; i != num; ++i) {
                chld[i] = chessBoard(*this);
                chld[i].procStep(av[i]);
                chld[i].from = av[i];
                chld[i].val();
            }
        }
        if (dsort) sort(chld, chld + num);
    }
    moveStep best() {
        stable_sort(chld, chld + num);
        return chld[0].from;
    }
    friend bool operator<(const storedNode &N1, const storedNode &N2) {
        return N1.storedVal < N2.storedVal;
    }
    void printAll() {
        for (uint16_t i = 0; i != num; ++i) {
            cout << chld[i].from << endl;
            chld[i].print();
            cout << int(chld[i].storedVal) << endl;
            cout << endl;
        }
    }
    void printDebug() {
        cout << num << " :: ";
        for (uint16_t i = 0; i != num; ++i)
            cout << chld[i].from << " : " << int(chld[i].storedVal) << " / ";
    }
};

class storedTree {
    storedNode root;
    int8_t depth;

 public:
    storedTree() {
        chessBoard B(cin);
        root = storedNode(B);
    }
    // ~storedTree() { root.deleteNode(); }
    void abDFS(storedNode &N, int8_t n, int8_t &x) {
        if (n == 0)
            N.val();
        else if (!(uint16_t(~N.num)) ||
                         (N.storedVal >= MIN0 && N.storedVal <= MAX0)) {
            N.storedVal = MIN1;
            N.calcAvail(n != depth);
            if (N.num == 0) N.end(depth - n);
            uint16_t i;
            for (i = 0; i != N.num; ++i) {
                abDFS(N.chld[i], n - 1, N.storedVal);
                if (N.storedVal > -x) {
                    ++i;
                    break;
                }
                if (timeUp) break;
            }
            for (; i < N.num; ++i) N.chld[i].storedVal = MAX0;
        }
        if (n == detectN)
            timeUp = (clock() - clk0) / double(CLOCKS_PER_SEC) > limitT;
        if (x < -N.storedVal) x = -N.storedVal;
    }
    moveStep alpha_beta(int8_t n) {
        root.calcAvail(true);
        int8_t ans = MIN1;
        abDFS(root, n, ans);
        if (root.storedVal <= MIN0 || root.storedVal >= MAX0) itSTOP = true;
        return root.best();
    }
    moveStep iteration() {
        clk0 = clock();
        depth = initDepth;
        moveStep best = alpha_beta(depth);
        while (!timeUp && !itSTOP) best = alpha_beta(++depth);
        return best;
    }
    void printDebug() {
        cout << int(depth) << " - ";
        root.printDebug();
    }
    void printBest() {
        cout << endl;
        storedNode *p = &root;
        while (true) {
            p->print();
            if (p->num == 0 || p->num == 0xFFFF) break;
            stable_sort(p->chld, p->chld + p->num);
            p = &(p->chld[0]);
        }
    }
};

int main() {
    istream::sync_with_stdio(false);
    storedTree T;
    moveStep ans = T.iteration();
    cout << ans << endl;
    T.printDebug();
    // T.printBest();
    return 0;
}
