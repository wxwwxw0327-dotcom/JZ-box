#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#ifndef MDFZ_TACTICAL_MATH_ONLY
#include "WhiteFleetDefenseCounterSwitch.h"
#endif

/** Strategy-only repair helpers. mm, ms, radians. No truth/SDK/key/model writes. */
namespace WhiteFleetMatchTacticalRepair
{
#ifndef MDFZ_TACTICAL_MATH_ONLY
  inline bool enabled(bool configured) {return configured && WhiteFleetDefenseCounterSwitch::enabled();}
#endif
  struct Point {float x=0.f,y=0.f;};
  inline bool finite(Point p) {return std::isfinite(p.x)&&std::isfinite(p.y);}
  inline float distance(Point a,Point b) {return std::hypot(a.x-b.x,a.y-b.y);}
  inline Point defensiveKickoffSlot(unsigned rank,float radius,float ownGoal,float halfWidth)
  {
    const float lateral=std::min(1500.f,std::max(350.f,halfWidth*.25f));
    return {std::max(ownGoal+600.f,-std::max(0.f,radius)-450.f-350.f*rank),
            (rank%2 ? 1.f:-1.f)*lateral};
  }
  /** Opponent kickoff, centre version: rank 0 blocks the straight pass lane just outside the centre circle,
   * rank 1 screens the middle in front of the own penalty area (the two lateral slots above leave the
   * centre lane open: a pass into it reached the opponent's runner in ~7 s). */
  inline Point centerKickoffSlot(unsigned rank,float radius,float ownGoal,float ownPenaltyX,float halfWidth)
  {
    if(rank==0)return {-std::max(0.f,radius)-850.f,0.f};
    if(rank==1)return {std::min(-std::max(0.f,radius)-1500.f,std::max(ownGoal+600.f,ownPenaltyX+750.f)),0.f};
    return defensiveKickoffSlot(rank,radius,ownGoal,halfWidth);
  }
  /** Observed opponent footprint for the own-kickoff first pass (radius already includes ball + margin). */
  struct LaneBlocker {Point center;float radius=0.f;};
  /** Free width (mm, negative = blocked) between the rolling ball path ball->target and the blockers in
   * front of the ball; a blocker beyond the target counts by its distance to the stop point. */
  inline float laneClearance(Point ball,Point target,const std::vector<LaneBlocker>& blockers)
  {
    const float dx=target.x-ball.x,dy=target.y-ball.y,d2=dx*dx+dy*dy;
    if(!finite(ball)||!finite(target)||!(d2>1.f))return -100000.f;
    float result=100000.f;
    for(const auto& b:blockers)
    {
      if(!finite(b.center)||!std::isfinite(b.radius)||b.radius<=0.f)continue;
      const float f=((b.center.x-ball.x)*dx+(b.center.y-ball.y)*dy)/d2;
      if(f<=0.f)continue;
      const float c=std::min(f,1.f);
      result=std::min(result,distance({ball.x+c*dx,ball.y+c*dy},b.center)-b.radius);
    }
    return result;
  }
  /** Own kickoff against a defender standing in the pass lane (e.g. a centre blocker just outside the circle):
   * the normal first pass (ray to ~(7.8 m,-1..-1.8 m)) is cut out at ~2.3 m on almost every kickoff.
   * Only when the normal lane is observed blocked or nearly so (clearance < 300 mm) pick a forward target on the receiver's
   * (right, y < ball) side whose lane clears every observed blocker by >= 400 mm and by 600 mm more than the
   * normal lane. Reachable stop distance only; invalid result = keep the normal pass. */
  struct WideLane {bool valid=false;Point target;float clearance=0.f;};
  inline WideLane wideKickoffLane(Point ball,float normalClearance,const std::vector<LaneBlocker>& blockers,
                                  float opponentGoal,float halfWidth,float maxStop,const Point* receiver)
  {
    WideLane best;
    if(blockers.empty()||!finite(ball)||!std::isfinite(normalClearance)||normalClearance>=300.f
       ||!std::isfinite(maxStop)||maxStop<2500.f||!std::isfinite(opponentGoal)||!std::isfinite(halfWidth))return best;
    float bestScore=-1e9f;
    for(float dx=3000.f;dx<=6500.f;dx+=500.f)
      for(float lateral=1500.f;lateral<=4500.f;lateral+=250.f)
      {
        const Point t{ball.x+dx,ball.y-lateral};
        if(t.x>opponentGoal-600.f||t.y<-halfWidth+900.f||distance(ball,t)>maxStop)continue;
        const float c=laneClearance(ball,t,blockers);
        if(c<400.f||c<normalClearance+600.f)continue;
        const float travel=receiver&&finite(*receiver)?distance(*receiver,t):0.f;
        // Enough width first, then deeper, less lateral and closer to the receiver.
        const float score=std::min(c,1000.f)+.05f*dx-.03f*lateral-.015f*travel;
        if(score>bestScore){bestScore=score;best={true,t,c};}
      }
    return best;
  }
  inline Point penaltyCorner(float penaltyX,float penaltyY,float goalX,float halfWidth,int side,float outside)
  {
    if(!std::isfinite(outside)||outside<100.f||outside>800.f)return {};
    const Point result{penaltyX-outside,(side<0?-1.f:1.f)*std::min(penaltyY+outside,halfWidth-600.f)};
    return finite(result)&&result.x>0.f&&result.x<goalX-600.f&&std::abs(result.y)>penaltyY ? result:Point{};
  }
  inline bool validKeeperLimits(float outside,float buffer)
  {return std::isfinite(outside)&&std::isfinite(buffer)&&outside>=0.f&&outside<=1500.f&&buffer>=0.f&&buffer<=outside;}
  inline float keeperMargin(Point p,float penaltyX,float penaltyHalfWidth,float outside)
  {
    const float x=p.x-penaltyX,y=std::abs(p.y)-penaltyHalfWidth;
    if(x>0.f || y>0.f)return outside-std::hypot(std::max(0.f,x),std::max(0.f,y));
    return outside+std::min(-x,-y); // Euclidean outer corners, not a +0.7m diagonal.
  }
  inline bool keeperMayPursue(Point ball,Point robot,float penaltyX,float penaltyHalfWidth,
                             float outside,float buffer,bool wasActive)
  {
    if(!finite(ball)||!finite(robot)||!validKeeperLimits(outside,buffer))return false;
    // Outer edge NEVER exceeds the configured +0.5m. Start at +0.3m;
    // continuation uses the remaining 0.2m, not a second outward allowance.
    return keeperMargin(ball,penaltyX,penaltyHalfWidth,outside)>=(wasActive?0.f:buffer)&&
           keeperMargin(robot,penaltyX,penaltyHalfWidth,outside)>=0.f;
  }
  inline float keeperApproachRatio(Point robot,float penaltyX,float penaltyHalfWidth,float outside,float buffer)
  {
    if(!finite(robot)||!validKeeperLimits(outside,buffer))return 0.f;
    const float margin=keeperMargin(robot,penaltyX,penaltyHalfWidth,outside);
    if(margin<=0.f)return 0.f;
    return buffer>0.f?std::clamp(margin/buffer,.25f,1.f):1.f;
  }
  inline Point keeperPreparation(Point desired,float penaltyX,float penaltyHalfWidth,float outside,float buffer)
  {
    const Point base{std::min(desired.x,penaltyX),std::clamp(desired.y,-penaltyHalfWidth,penaltyHalfWidth)};
    if(!finite(desired)||!validKeeperLimits(outside,buffer))return base;
    const float length=distance(base,desired),limit=outside-buffer;
    const float ratio=length>limit&&length>0.f?limit/length:1.f;
    return {base.x+(desired.x-base.x)*ratio,base.y+(desired.y-base.y)*ratio};
  }
  inline float segmentDistance(Point p,Point a,Point b)
  {
    const float x=b.x-a.x,y=b.y-a.y,n=x*x+y*y;
    const float t=n>1.f?std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/n,0.f,1.f):0.f;
    return distance(p,{a.x+t*x,a.y+t*y});
  }
  inline bool powerAdvance(bool profile,bool ordinary,bool direct,bool reliable,bool upright,
                           int ballAge,Point ball,float goalX,float localDistance,float ballSpeed,
                           bool nativeStrong,float plannedDistance,bool pressureFree)
  {
    return profile&&ordinary&&direct&&reliable&&upright&&nativeStrong&&pressureFree&&
      ballAge>=0&&ballAge<=200&&finite(ball)&&std::isfinite(goalX)&&goalX>0.f&&
      ball.x<=goalX/3.f&&std::isfinite(localDistance)&&localDistance>=0.f&&localDistance<=1000.f&&
      std::isfinite(ballSpeed)&&ballSpeed>=0.f&&ballSpeed<=1000.f&&
      std::isfinite(plannedDistance)&&plannedDistance>=6000.f;
  }
  inline float goalSectorShift(bool profile,bool ordinary,bool reliable,int ballAge,
                               float ballX,float penaltyX,float goalX,float localDistance,float configured)
  {
    if(!profile||!ordinary||!reliable||ballAge<0||ballAge>200||!std::isfinite(ballX)||
       !std::isfinite(penaltyX)||!std::isfinite(goalX)||!std::isfinite(localDistance)||
       localDistance<0.f||localDistance>1000.f||ballX<penaltyX-2000.f||ballX>=goalX||
       !std::isfinite(configured)||configured<0.f||configured>=.5f)return configured;
    // Aim within the central 10% of the SAME native free goal sector.
    // No random/fixed side angle, no obstacle culling or validity override.
    return std::max(configured,.45f);
  }
  inline Point kickoffStaging(Point target,float halfway,float lineWidth,float largeMargin,float halfWidth)
  {
    const float clearance=std::max(0.f,largeMargin)+std::max(0.f,lineWidth)*.5f+50.f;
    const float lateral=std::max(0.f,halfWidth-clearance);
    return {std::min(target.x,halfway-clearance),std::clamp(target.y,-lateral,lateral)};
  }
  enum RestartReason {restartNone=0,waitObservation=1,waitPeer=2,waitSafety=3,preparing=4,
    passAdmitted=5,ourRelease=6,externalRelease=7,budgetExpired=8,receiving=9,waitArrival=10,unconfirmedRelease=11};
  constexpr unsigned kickoffContractBudget=20000,goalKickContractBudget=35000,strikeBudget=3000;
  class RestartAnchor
  {
  public:
    unsigned since=0,firstSeen=0,lastSeen=0;Point point{},seed{};
    bool update(unsigned now,unsigned seen,unsigned epoch,Point observation,bool allowed)
    {
      if(!allowed||!now||!seen||seen<epoch||seen>now||now-seen>200||!finite(observation))
      {*this=RestartAnchor{};return false;}
      if(!since||distance(point,observation)>150.f||distance(seed,observation)>150.f||seen<lastSeen)
      {since=now;firstSeen=seen;seed=observation;}
      point=observation;lastSeen=seen;
      return now>=since&&now-since>=120&&seen>firstSeen;
    }
  };
  inline bool restartReleased(unsigned now,unsigned issued,unsigned event,unsigned seen,
                              Point origin,Point target,Point ball)
  {
    if(!issued||event<issued||event>now||seen<=event||seen>now||now-seen>200||
       !finite(origin)||!finite(target)||!finite(ball))return false;
    const Point travel{ball.x-origin.x,ball.y-origin.y},plan{target.x-origin.x,target.y-origin.y};
    const float length=distance(origin,target);
    return length>500.f&&distance(origin,ball)>500.f&&travel.x*plan.x+travel.y*plan.y>0.f&&
      std::abs(travel.x*plan.y-travel.y*plan.x)/length<750.f;
  }
  inline Point farReceiverMeeting(Point ball,Point peer)
  {
    if(!finite(ball)||!finite(peer)||peer.x<=ball.x+2500.f)return {};
    const float length=distance(ball,peer);
    if(length<3000.f)return {};
    const float ratio=length>12000.f?11500.f/length:1.f;
    return {ball.x+(peer.x-ball.x)*ratio,ball.y+(peer.y-ball.y)*ratio};
  }
  inline bool forwardOutlet(Point ball,Point peer,float goalX,float halfWidth)
  {
    const float d=distance(ball,peer);
    return finite(ball)&&finite(peer)&&peer.x>ball.x+2500.f&&d>=3000.f&&d<=12000.f&&
           peer.x<goalX-600.f&&std::abs(peer.y)<halfWidth-600.f;
  }

  class PursuitSession
  {
  public:
    void reset()
    {
      const unsigned consumed=std::max(consumedWitness,started);
      *this=PursuitSession{};consumedWitness=consumed;
    }
    bool update(unsigned now,unsigned witness,unsigned seen,bool allowed,Point robot,float ballDistance)
    {
      if(!allowed || !now || !seen || seen>now || now-seen>300 || !finite(robot) ||
         !std::isfinite(ballDistance) || ballDistance<750.f || (lastNow && (now<=lastNow || now-lastNow>250)))
      {reset();return false;}
      lastNow=now;
      if(witness && witness<=now && now-witness<1200 && witness>consumedWitness)
      {started=consumedWitness=witness;lastProgress=now;progressRobot=robot;}
      if(!started)return false;
      if(distance(robot,progressRobot)>30.f)
      {lastProgress=now;progressRobot=robot;}
      if(now-started>=5000 || now-lastProgress>600){reset();return false;}
      return true;
    }
    unsigned witness()const{return started;}
  private:
    unsigned started=0,lastNow=0,lastProgress=0,consumedWitness=0;
    Point progressRobot;
  };

  /** One short quarantine for an impossible near-to-far post-kick innovation.
   * Does not label a detection false, rewrite it, or supply an alternative ball.
   * Raw Color/model ambiguity remains a perception validation item.
   */
  class ObservationGuard
  {
  public:
    void reset(){*this=ObservationGuard{};}
    bool update(unsigned now,unsigned seen,unsigned kick,bool allowed,Point ball,Point robot,float rotation)
    {
      if(!allowed || !now || !finite(ball) || !finite(robot) || !std::isfinite(rotation) ||
         seen>now || kick>now || (lastNow && (now<=lastNow || now-lastNow>250)))
      {reset();return false;}
      lastNow=now;
      const Point observed{robot.x+std::cos(rotation)*ball.x-std::sin(rotation)*ball.y,
                           robot.y+std::sin(rotation)*ball.x+std::cos(rotation)*ball.y};
      if(!seen || now-seen>200)return false;
      // Keep the last actually observed near-foot anchor independently of
      // far candidates. A one-tick-late MotionInfo must not let a suspect
      // first far observation erase the evidence before the event arrives.
      if(seen>nearSeen && std::hypot(ball.x,ball.y)<=650.f)
      {near=observed;nearSeen=seen;}
      if(kick && kick>=nearSeen && now-kick<1200 && kick!=consumedKick &&
         nearSeen && seen>nearSeen && seen-nearSeen<400 &&
         distance(observed,near)>800.f+10.f*(seen-nearSeen))
      {consumedKick=kick;holdUntil=now+650;}
      if(holdUntil && now<holdUntil)
      {
        if(distance(observed,near)<650.f)holdUntil=0;
        else return true;
      }
      return false;
    }
  private:
    unsigned nearSeen=0,consumedKick=0,holdUntil=0,lastNow=0;
    Point near;
  };
}
