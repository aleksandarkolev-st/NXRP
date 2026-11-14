// Adventure systems share the renderer's sourced-mesh loading helpers.
void GameView::previewGather()
{
    const int cache=nearbyCache();
    if(networkGuest_ && cache>=0) {adventureMotionClock_=1.2f;adventureMotionItem_=-1;lootingCache_=cache;}
}
int GameView::adventureHouse() const
{
    return safehouseAt({static_cast<int>(z_),static_cast<int>(x_)},safehouses_);
}

int GameView::nearbyCache() const
{
    if(elevation_>.1f) return -1;
    for(std::size_t i=0;i<caches_.size();++i)
    {
        if(openedCaches_&(std::uint64_t{1}<<i)) continue;
        const auto p=caches_[i].cell;
        const float dx=p.col+.5f-x_,dz=p.row+.5f-z_;
        const float distance=std::sqrt(dx*dx+dz*dz);
        if(distance<.72f && (distance<.15f || (dx*std::sin(angle_)+dz*std::cos(angle_))/distance>.2f)) return static_cast<int>(i);
    }
    return -1;
}

void GameView::recallAdventurer(int actor,GameState& game)
{
    const int house=checkpoints_[actor];if(house<0) return;
    const auto destination=safehouses_[house];
    for(auto& houseBalls:refugeBalls_)for(auto& ball:houseBalls)if(ball.carrier==actor){ball.carrier=-1;ball.throwWindup=0.f;}
    if(actor==networkSelfSlot_)
    {
        x_=destination.col+.5f;z_=destination.row+.5f;elevation_=jumpHeight_=jumpVelocity_=0.f;
        seatedHouse_=-1;drinkClock_=0.f;game.player=destination;seenRecall_=packs_[actor].recalls;
    }
    else
    {
        auto& player=remote_[actor];player.x=destination.col+.5f;player.z=destination.row+.5f;
        player.elevation=player.jumpHeight=0.f;player.seated=false;
    }
}

void GameView::useAdventureItem(int actor,int item,GameState& game)
{
    auto& pack=packs_[actor];
    if(pack.useCooldown>0.f) {notice_="Finish the current invocation first.";return;}
    if(!pack.items[item]) {notice_="That slot is empty. Brew or buy another.";return;}
    if(item==4 && checkpoints_[actor]<0) {notice_="Visit a trading post before using Recall.";return;}
    --pack.items[item];
    pack.useCooldown=1.2f;
    if(actor==networkSelfSlot_) {adventureMotionClock_=1.2f;adventureMotionItem_=item;drinkClock_=0.f;}
    if(item==0)
    {
        ++pack.restores;
        if(actor==networkSelfSlot_) {seenRestore_=pack.restores;stamina_=1.f;fear_=std::max(0.f,fear_-.3f);}
    }
    if(item>=1 && item<=3) pack.effects[item-1]=item==1?12.f:item==2?15.f:20.f;
    if(item==4) {++pack.recalls;recallAdventurer(actor,game);}
    if(item==5)
    {
        ++pack.echoes;
        auto player=playerAt(actor);pack.echo={static_cast<int>(player.z),static_cast<int>(player.x)};
        for(float t=.2f;t<4.f;t+=.2f)
        {
            const float x=player.x+std::sin(player.angle)*t,z=player.z+std::cos(player.angle)*t;
            const MazePosition at{static_cast<int>(z),static_cast<int>(x)};
            if(!game.maze.contains(at) || game.maze.at(at.row,at.col)==MazeCell::Wall || !refugeEdgeAllowed(pack.echo,at,safehouses_)) break;
            pack.echo=at;
        }
        pack.echoTime=8.f;
    }
    notice_=std::string("Used ")+Adventure::itemNames[item]+". "+Adventure::descriptions[item];
}

bool GameView::adventureEvent(sf::Keyboard::Key key,GameState& game)
{
    const int actor=networkGuest_?networkSelfSlot_:interactionSlot_;
    auto& panel=adventurePanels_[actor];
    int requested=key==sf::Keyboard::Key::I?1:key==sf::Keyboard::Key::O?2:key==sf::Keyboard::Key::N?3:key==sf::Keyboard::Key::Tab?4:0;
    if(requested)
    {
        if(requested==2 && nearbyBar()<0) {notice_="Find the keeper's counter in a trading post. O to trade.";messageClock_=4.f;return true;}
        if(requested==3 && adventureHouse()<0) {notice_="Brewing needs a trading-post workbench. N to craft there.";messageClock_=4.f;return true;}
        panel=panel==requested?0:requested;
        if(actor==networkSelfSlot_) pokerVisible_=false;
        return true;
    }
    if(key==sf::Keyboard::Key::Q && panel) {panel=0;return true;}
    if(!panel) return false;
    const sf::Keyboard::Key top[]={sf::Keyboard::Key::Num1,sf::Keyboard::Key::Num2,sf::Keyboard::Key::Num3,sf::Keyboard::Key::Num4,sf::Keyboard::Key::Num5,sf::Keyboard::Key::Num6,sf::Keyboard::Key::Num7};
    const sf::Keyboard::Key pad[]={sf::Keyboard::Key::Numpad1,sf::Keyboard::Key::Numpad2,sf::Keyboard::Key::Numpad3,sf::Keyboard::Key::Numpad4,sf::Keyboard::Key::Numpad5,sf::Keyboard::Key::Numpad6,sf::Keyboard::Key::Numpad7};
    int selected=-1;for(int i=0;i<7;++i) if(key==top[i] || key==pad[i]) selected=i;
    if(selected<0) return false;
    if(networkGuest_ || panel==4) return true;
    auto& pack=packs_[actor];
    if(panel==1 && selected<6) useAdventureItem(actor,selected,game);
    else if(panel==2 && nearbyBar()>=0)
    {
        const bool done=selected==6?Adventure::sell(pack):Adventure::buy(pack,selected,adventureHouse());
        notice_=done?(selected==6?"Sold a bundle of ingredients for 9 crowns.":std::string("Purchased ")+Adventure::itemNames[selected]+"."):"Not enough crowns or ingredients. Explore for more.";
    }
    else if(panel==3 && selected<6 && adventureHouse()>=0)
        notice_=Adventure::craft(pack,selected)?std::string("Crafted ")+Adventure::itemNames[selected]+".":"Missing ingredients. Search caches or try another recipe.";
    else notice_="Return to the trading post to finish that transaction.";
    messageClock_=4.f;return true;
}

void GameView::updateAdventure(const GameState& game,float seconds)
{
    for(auto& pack:packs_) Adventure::tick(pack,seconds);
    adventureMotionClock_=std::max(0.f,adventureMotionClock_-seconds);
    for(auto& timer:cacheOpenClocks_)timer=std::max(0.f,timer-seconds);
    if(!networkGuest_) for(int actor=0;actor<maxCoopPlayers;++actor)
    {
        const auto player=playerAt(actor);if(!player.present) continue;
        const MazePosition at{static_cast<int>(player.z),static_cast<int>(player.x)};
        const unsigned district=1u<<storyDistrict(at,game.maze.size());
        auto& pack=packs_[actor];
        if(!(pack.districts&district))
        {
            pack.districts|=district;pack.coins=std::min(99999u,pack.coins+4);
            if(actor==networkSelfSlot_) {notice_="New district discovered. +4 crowns. Tab opens your atlas.";messageClock_=4.f;}
        }
    }
    const auto& pack=packs_[networkSelfSlot_];
    if(pack.effects[1]>0.f) {stamina_=std::min(1.f,stamina_+seconds*.8f);exhausted_=false;fear_=std::max(0.f,fear_-seconds*.06f);}
    if(surveyed_.size()!=game.maze.cellCount()) surveyed_.assign(game.maze.cellCount(),false);
    const int cols=game.maze.size().cols;
    const MazePosition start{static_cast<int>(z_),static_cast<int>(x_)};
    std::vector<std::pair<MazePosition,int>> queue{{start,0}};
    std::vector<MazePosition> visited{start};
    constexpr MazePosition steps[]={{-1,0},{1,0},{0,-1},{0,1}};
    for(std::size_t i=0;i<queue.size();++i)
    {
        const auto p=queue[i].first;surveyed_[p.row*cols+p.col]=true;
        if(queue[i].second>=3) continue;
        for(auto d:steps)
        {
            const MazePosition n{p.row+d.row,p.col+d.col};if(!game.maze.contains(n)) continue;
            surveyed_[n.row*cols+n.col]=true;
            if(game.maze.at(n.row,n.col)==MazeCell::Wall || !refugeEdgeAllowed(p,n,safehouses_)) continue;
            if(std::none_of(visited.begin(),visited.end(),[&](MazePosition v){return v.row==n.row && v.col==n.col;}))
            {visited.push_back(n);queue.push_back({n,queue[i].second+1});}
        }
    }
}

void GameView::drawAdventure(const GameState& game)
{
    if(!adventureAssetsAttempted_)
    {
        const auto root=executableDirectory()/"assets"/"models";
        const char* assetNames[]={"adventure-potion","adventure-chest","adventure-chest-body","adventure-chest-lid","adventure-lantern"};
        for(int i=0;i<5;++i) {adventureMeshes_[i]=loadCreatureMesh(root/(std::string(assetNames[i])+".nxm"));adventureTextures_[i]=loadSurfaceTexture(root/(std::string(assetNames[i])+".png"),1024);}
        adventureAssetsAttempted_=true;
    }
    sf::Shader::bind(nullptr);glEnable(GL_TEXTURE_2D);
    const GLfloat cacheColors[3][4]={{.12f,.28f,.13f,1},{.12f,.22f,.36f,1},{.32f,.16f,.05f,1}};
    for(std::size_t i=0;i<caches_.size();++i)
    {
        const bool opened=openedCaches_&(std::uint64_t{1}<<i);
        const auto cache=caches_[i];const float x=cache.cell.col+.5f,z=cache.cell.row+.5f;
        if((x-x_)*(x-x_)+(z-z_)*(z-z_)>100.f) continue;
        glPushMatrix();glTranslatef(x,.01f,z);glRotatef(static_cast<float>((i*73)%360),0,1,0);
        glColor3f(1,1,1);glBindTexture(GL_TEXTURE_2D,adventureTextures_[1]);
        if(adventureMeshes_[2] && adventureMeshes_[3])
        {
            glCallList(adventureMeshes_[2]);glPushMatrix();
            const float lift=opened?interactionSmooth((.95f-cacheOpenClocks_[i])/.65f):0.f;
            glTranslatef(0,.18f,-.12f);glRotatef(-100.f*lift,1,0,0);glTranslatef(0,-.18f,.12f);
            glCallList(adventureMeshes_[3]);glPopMatrix();
        }
        else if(adventureMeshes_[1])glCallList(adventureMeshes_[1]);
        if(opened) {glPopMatrix();continue;}
        glTranslatef(0,.34f,0);glScalef(.5f,.5f,.5f);
        glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,cacheColors[cache.kind]);
        glBindTexture(GL_TEXTURE_2D,adventureTextures_[0]);if(adventureMeshes_[0])glCallList(adventureMeshes_[0]);
        const GLfloat none[]={0,0,0,1};glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,none);glPopMatrix();
    }
    for(std::size_t house=0;house<safehouses_.size();++house)
    {
        const auto at=safehouses_[house];const float hx=at.col+.5f,hz=at.row+.5f;
        if(std::abs(hx-x_)>9 || std::abs(hz-z_)>9)continue;
        // Distinct arrangements of existing/downloaded props; no authored meshes.
        for(int prop=0;prop<4;++prop)
        {
            glPushMatrix();glTranslatef(hx-1.35f,.65f,hz+.9f+prop*.19f);glScalef(.35f,.35f,.35f);
            const int kind=house==0?0:house==1?1:house==2?2:prop%3;
            glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,cacheColors[kind]);glBindTexture(GL_TEXTURE_2D,adventureTextures_[0]);
            if(adventureMeshes_[0])glCallList(adventureMeshes_[0]);glPopMatrix();
        }
        const GLfloat noGlow[]={0,0,0,1};glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,noGlow);
        glPushMatrix();glTranslatef(hx+1.35f,0,hz-1.35f);glRotatef(35,0,1,0);
        if(house==1) {glBindTexture(GL_TEXTURE_2D,storyTextures_[1]);if(storyMeshes_[1])glCallList(storyMeshes_[1]);}
        else {glBindTexture(GL_TEXTURE_2D,adventureTextures_[4]);if(adventureMeshes_[4])glCallList(adventureMeshes_[4]);}
        glPopMatrix();
        if(adventureSigns_[house%4] && storyMeshes_[4])
        {
            sf::Texture::bind(&adventureSigns_[house%4]->getTexture());glPushMatrix();
            glTranslatef(hx,1.86f,hz-2.36f);glScalef(2.3f,2.3f,2.3f);glCallList(storyMeshes_[4]);glPopMatrix();sf::Texture::bind(nullptr);
        }
    }
    // Reuse the downloaded survivor's idle frame as the keeper at each counter.
    if(survivorMesh_ && survivorFrames_>1)
    for(const auto house:safehouses_)
    {
        if(std::abs(house.col+.5f-x_)>9 || std::abs(house.row+.5f-z_)>9) continue;
        glPushMatrix();glTranslatef(house.col+.5f-1.85f,0,house.row+.5f+1.35f);glRotatef(90.f,0,1,0);
        glColor3f(.82f,.91f,.85f);glBindTexture(GL_TEXTURE_2D,survivorTexture_);glCallList(survivorMesh_+survivorFrames_-1);glPopMatrix();
    }
    glColor3f(1,1,1);glDisable(GL_TEXTURE_2D);
    // Wayfinder particles follow real passable cells. Particles are effects,
    // while all solid pickups/keepers remain downloaded source meshes.
    if(packs_[networkSelfSlot_].effects[2]>0.f)
    {
        MazePosition goal=solved_<3?shrines_[puzzle_.stoneKind==StonePuzzleKind::Sequence?order_[solved_]:0]:MazePosition{game.maze.size().rows-1,game.maze.size().cols-1};
        if(puzzle_.stoneKind==StonePuzzleKind::Balance && solved_<3)
        {for(int i=0;i<3;++i) if(!(activeStones_&(1u<<i))) {goal=shrines_[i];break;}}
        const int cols=game.maze.size().cols,total=static_cast<int>(game.maze.cellCount());
        std::vector<int> parent(total,-1),queue;const int begin=static_cast<int>(z_)*cols+static_cast<int>(x_);
        parent[begin]=begin;queue.push_back(begin);const int end=goal.row*cols+goal.col;
        constexpr MazePosition ds[]={{-1,0},{1,0},{0,-1},{0,1}};
        for(std::size_t i=0;i<queue.size() && parent[end]<0;++i)
        {
            const MazePosition p{queue[i]/cols,queue[i]%cols};
            for(auto d:ds)
            {
                const MazePosition n{p.row+d.row,p.col+d.col};
                if(!game.maze.contains(n) || game.maze.at(n.row,n.col)==MazeCell::Wall || !refugeEdgeAllowed(p,n,safehouses_)) continue;
                const int at=n.row*cols+n.col;if(parent[at]>=0) continue;
                parent[at]=queue[i];queue.push_back(at);
            }
        }
        if(parent[end]>=0)
        {
            std::vector<int> path;for(int at=end;at!=begin;at=parent[at])path.push_back(at);std::reverse(path.begin(),path.end());
            glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);glPointSize(9.f);
            glBegin(GL_POINTS);for(std::size_t i=0;i<std::min(path.size(),std::size_t{12});++i)
            {glColor4f(.36f,.85f,1.f,.45f+.3f*std::sin(ambientTime_*3.f-i*.7f));glVertex3f(path[i]%cols+.5f,.24f+.04f*std::sin(ambientTime_*2.f+i),path[i]/cols+.5f);}glEnd();
            glPointSize(1);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_LIGHTING);
        }
    }
}

void GameView::drawAdventureHud(sf::RenderTarget& target,const GameState& game)
{
    const auto& pack=packs_[networkSelfSlot_];const int mode=adventurePanels_[networkSelfSlot_];
    sf::Text text(heading_.getFont(),"",16);
    const auto label=[&](const std::string& value,float x,float y,unsigned size,sf::Color color)
    {text.setString(value);text.setCharacterSize(size);text.setFillColor(color);text.setPosition({x,y});target.draw(text);};
    const sf::Color gold{229,199,133},pale{222,232,215},muted{151,175,166};
    if(!mode)
    {
        std::string status=std::to_string(pack.coins)+" crowns / I satchel / Tab atlas";
        label(status,24,viewport_.y-131.f,15,gold);
        std::string effects;
        if(pack.effects[0]>0)effects+="VEIL "+std::to_string(static_cast<int>(std::ceil(pack.effects[0])))+"s   ";
        if(pack.effects[1]>0)effects+="FLEETFOOT   ";if(pack.effects[2]>0)effects+="WAYFINDER";
        if(!effects.empty())label(effects,24,viewport_.y-153.f,15,{133,205,220});
        if(nearbyCache()>=0) label("E / OPEN WAYFARER CACHE",24,110,20,gold);
        else if(nearbyBar()>=0)label("O trade / N brew / I satchel",24,108,18,gold);
        return;
    }
    const float width=std::min(860.f,viewport_.x-32.f),left=(viewport_.x-width)*.5f,top=86.f;
    const float height=std::min(466.f,viewport_.y-110.f);
    sf::RectangleShape shadow({width+8,height+8});shadow.setPosition({left+3,top+4});shadow.setFillColor({0,0,0,150});target.draw(shadow);
    sf::RectangleShape panel({width,height});panel.setPosition({left,top});panel.setFillColor({13,29,29,248});panel.setOutlineColor({137,119,75});panel.setOutlineThickness(1);target.draw(panel);
    const char* titles[]={"","THE WAYFARER'S SATCHEL","THE LANTERN EXCHANGE","THE BREWER'S WORKBENCH","ATLAS OF THE LIVING HEDGE"};
    const int currentHouse=adventureHouse();
    label(mode==2 && currentHouse>=0?Adventure::shopNames[currentHouse%4]:titles[mode],left+20,top+13,25,gold);
    label(std::to_string(pack.coins)+" CROWNS   /   "+std::to_string(pack.materials[0])+" dewleaf   "+std::to_string(pack.materials[1])+" moon salt   "+std::to_string(pack.materials[2])+" brass",left+20,top+48,16,muted);
    if(mode==4)
    {
        const float cell=std::min((width-48)/game.maze.size().cols,(height-130)/game.maze.size().rows);
        const float ox=left+(width-cell*game.maze.size().cols)*.5f,oy=top+82;
        sf::RectangleShape tile({std::max(1.f,cell-.5f),std::max(1.f,cell-.5f)});
        for(int r=0;r<game.maze.size().rows;++r)for(int c=0;c<game.maze.size().cols;++c)
        {
            if(surveyed_.empty() || !surveyed_[r*game.maze.size().cols+c])continue;
            tile.setPosition({ox+c*cell,oy+r*cell});tile.setFillColor(game.maze.at(r,c)==MazeCell::Wall?sf::Color(43,63,61):sf::Color(103,137,121));target.draw(tile);
        }
        const auto marker=[&](MazePosition p,sf::Color color)
        {if(surveyed_.empty() || !surveyed_[p.row*game.maze.size().cols+p.col])return;sf::CircleShape dot(std::max(2.f,cell*.25f));dot.setFillColor(color);dot.setOrigin({dot.getRadius(),dot.getRadius()});dot.setPosition({ox+(p.col+.5f)*cell,oy+(p.row+.5f)*cell});target.draw(dot);};
        for(auto house:safehouses_)marker(house,gold);for(auto stone:shrines_)marker(stone,{122,196,245});
        for(std::size_t i=0;i<caches_.size();++i)if(!(openedCaches_&(std::uint64_t{1}<<i)))marker(caches_[i].cell,{149,215,134});
        marker({static_cast<int>(z_),static_cast<int>(x_)},{255,247,206});
        label("Gold: trading posts / Blue: stones / Green: caches / White: you",left+20,top+height-52,15,muted);
    }
    else for(int i=0;i<6;++i)
    {
        const float y=top+79+i*47.f;
        sf::RectangleShape stripe({width-32,43});stripe.setPosition({left+16,y});stripe.setFillColor(i%2?sf::Color(24,43,40):sf::Color(19,36,35));target.draw(stripe);
        label(std::to_string(i+1)+"   "+Adventure::itemNames[i],left+24,y+2,18,pale);
        label(Adventure::descriptions[i],left+47,y+24,14,muted);
        std::string detail=mode==1?"x"+std::to_string(pack.items[i]):mode==2?std::to_string(Adventure::price(i,currentHouse))+" crowns":std::to_string(Adventure::recipes[i][0])+" D / "+std::to_string(Adventure::recipes[i][1])+" M / "+std::to_string(Adventure::recipes[i][2])+" B";
        label(detail,left+width-155,y+4,16,gold);
    }
    if(mode==2)label("7 / Sell one of each ingredient for 9 crowns",left+24,top+height-56,16,gold);
    else if(mode==3)label("D: dewleaf / M: moon salt / B: old brass",left+24,top+height-56,15,muted);
    if(messageClock_>0.f && mode!=4)
    {
        std::string result=notice_;if(result.size()>67)result=result.substr(0,64)+"...";
        label(result,left+24,top+height-78,14,gold);
    }
    label(mode==1?"1-6 use / O trade / N brew / Q close - the world keeps moving":mode==4?"Only explored routes are shown. Tab or Q closes the atlas.":"1-6 select / I satchel / Q close - the world keeps moving",left+20,top+height-28,14,pale);
}
