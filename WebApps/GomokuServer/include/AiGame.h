#pragma once

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <mutex>

//q 管理每局棋盘状态

const int BOARD_SIZE = 15;//q 默认棋盘大小15*15

const std::string EMPTY = "empty";       // q 空位置
const std::string AI_PLAYER = "white";   // AI玩家白棋
const std::string HUMAN_PLAYER = "black"; // 人类玩家黑棋

class AiGame
{
public:
    AiGame(int userId);

    // 判断是否平局
    bool isDraw() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return moveCount_ >= BOARD_SIZE * BOARD_SIZE;//q 后面每下一步棋会++moveCount_,
                                                     //q 当moveCount_>=棋盘还没有结束，判定平局
    }

    bool humanMove(int x, int y);

    bool checkWin(int x,int y, const std::string& player);

    void aiMove();

    //q 后续的这几个获取状态的函数均要加锁之后进行操作 是因为防止多线程读写时发生数据竞争

    // 获取最后一步移动的坐标
    std::pair<int, int> getLastMove() const 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastMove_;
    }

     // 获取当前棋盘状态
    const std::vector<std::vector<std::string>>& getBoard() const 
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        return board_; 
    }

    bool isGameOver() const 
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        return gameOver_; 
    }

    std::string getWinner() const 
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        return winner_; 
    }

private:
    // 检查移动是否有效
    bool isValidMove(int x, int y) const 
    {
        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) return false; //q 超出棋盘范围
        if (board_[x][y] != EMPTY) return false;//q 在已有棋子位置落子
        if (gameOver_ || isDraw()) return false;//q 已经游戏结束或者平局
        return true;
    }

     // 检查坐标是否在棋盘内
    bool isInBoard(int x, int y) const 
    {
        return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
    }

    // 获取AI的最佳移动位置
    std::pair<int, int> getBestMove();
    // 评估威胁 
    int evaluateThreat(int r, int c);
    // 判断某个空位是否靠近已有棋子
    bool isNearOccupied(int r, int c);

private:
    bool                                  gameOver_;//q 游戏是否结束？
    int                                   userId_;//q 用户id 用于标识不同用户的棋盘
    int                                   moveCount_;//q 已经落子的数量
    std::string                           winner_{"none"};
    std::pair<int, int>                   lastMove_{-1, -1};  // 上一次落子位置
    std::vector<std::vector<std::string>> board_; //q 棋盘
    mutable std::mutex                    mutex_;  // 添加互斥锁
};