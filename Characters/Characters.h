#include <SFML/Graphics.hpp>
enum class CharacterType
{
    ENEMY,
    PLAYER

};
enum class ActiveStatus
{
    ACTIVE,
    STUNNED
};
class Character
{
    protected:
    CharacterType characterType;
    //character properties
    int Hp;
    int maxHp;
    int demage;
    int stamina;
    int MaxStamina;
    //status vars
    bool alive;
    bool myTurn;
    //stuning shi
    bool stunned;
    int stunEndTem;
    //ISHOW SPEED
    float speed;
    //coordinates
    sf::Vector2f pos;

    public:

    //main action mechanism ig
    virtual void DoAction()=0;
    virtual ~Character(){};
    //Helth related shi
    void TakeDemage(int amount)
    {

    }
    void RegainHealth(int rate){}
    bool amAlive() const{}

    //Stamina
    void ResetStamina()
    {

    }
    bool CanAct(){} //check stamina is full type shi
    void UpdateStamina(){} // called for every player every cycle to update stamina

    //stunning
    void StunMeh(float duration){}
    void NonStunMen(){}
    bool amStunned () const{}

    //Getters and Setters
    int getHp()
    {
        return Hp;
    }
    void setHp(int hp)
    {
        Hp = hp;
    }
    int getMaxHp()
    {
        return maxHp;
    }
    void setMaxHp(int maxHp)
    {
        this->maxHp = maxHp;
    }
    int getDemage()
    {
        return demage;
    }
    void setDemage(int demage)
    {
        this->demage = demage;
    }
    int getStamina()
    {
        return stamina;
    }
    void setStamina(int stamina)
    {
        this->stamina = stamina;
    }
    int getMaxStamina()
    {
        return MaxStamina;
    }
    void setMaxStamina(int maxStamina)
    {
        MaxStamina = maxStamina;
    }
    bool isAlive()
    {
        return alive;
    }
    void setAlive(bool alive)    {
        this->alive = alive;
    }
    bool isMyTurn()
    {
        return myTurn;
    }
    void setMyTurn(bool myTurn)
    {
        this->myTurn = myTurn;
    }
    bool isStunned()
    {
        return stunned;
    }
    void setStunned(bool stunned)
    {
        this->stunned = stunned;
    }
    int getStunEndTem()
    {
        return stunEndTem;
    }
    void setStunEndTem(int stunEndTem)
    {        this->stunEndTem = stunEndTem;
    }
    float getSpeed()
    {
        return speed;
    }
    void setSpeed(float speed)
    {
        this->speed = speed;
    }
    float getXPos()
    {
        return X_pos;
    }
    void setXPos(float xPos)
    {
           X_pos = xPos;
    }
    float getYPos()
    {
            return Y_pos;
    }
    void setYPos(float yPos)
    {
         Y_pos = yPos;
    }





};
