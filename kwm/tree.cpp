#include "tree.h"
#include "helpers.h"
#include "display.h"
#include "space.h"
#include "window.h"
#include "border.h"

extern kwm_path KWMPath;
extern kwm_screen KWMScreen;
extern kwm_tiling KWMTiling;

node_container LeftVerticalContainerSplit(screen_info *Screen, tree_node *Node)
{
    Assert(Node, "LeftVerticalContainerSplit()")
    space_info *Space = GetActiveSpaceOfScreen(Screen);
    node_container LeftContainer;

    LeftContainer.X = Node->Container.X;
    LeftContainer.Y = Node->Container.Y;
    LeftContainer.Width = (Node->Container.Width * Node->SplitRatio) - (Space->Settings.Offset.VerticalGap / 2);
    LeftContainer.Height = Node->Container.Height;

    return LeftContainer;
}

node_container RightVerticalContainerSplit(screen_info *Screen, tree_node *Node)
{
    Assert(Node, "RightVerticalContainerSplit()")
    space_info *Space = GetActiveSpaceOfScreen(Screen);
    node_container RightContainer;

    RightContainer.X = Node->Container.X + (Node->Container.Width * Node->SplitRatio) + (Space->Settings.Offset.VerticalGap / 2);
    RightContainer.Y = Node->Container.Y;
    RightContainer.Width = (Node->Container.Width * (1 - Node->SplitRatio)) - (Space->Settings.Offset.VerticalGap / 2);
    RightContainer.Height = Node->Container.Height;

    return RightContainer;
}

node_container UpperHorizontalContainerSplit(screen_info *Screen, tree_node *Node)
{
    Assert(Node, "UpperHorizontalContainerSplit()")
    space_info *Space = GetActiveSpaceOfScreen(Screen);
    node_container UpperContainer;

    UpperContainer.X = Node->Container.X;
    UpperContainer.Y = Node->Container.Y;
    UpperContainer.Width = Node->Container.Width;
    UpperContainer.Height = (Node->Container.Height * Node->SplitRatio) - (Space->Settings.Offset.HorizontalGap / 2);

    return UpperContainer;
}

node_container LowerHorizontalContainerSplit(screen_info *Screen, tree_node *Node)
{
    Assert(Node, "LowerHorizontalContainerSplit()")
    space_info *Space = GetActiveSpaceOfScreen(Screen);
    node_container LowerContainer;

    LowerContainer.X = Node->Container.X;
    LowerContainer.Y = Node->Container.Y + (Node->Container.Height * Node->SplitRatio) + (Space->Settings.Offset.HorizontalGap / 2);
    LowerContainer.Width = Node->Container.Width;
    LowerContainer.Height = (Node->Container.Height * (1 - Node->SplitRatio)) - (Space->Settings.Offset.HorizontalGap / 2);

    return LowerContainer;
}

void CreateNodeContainer(screen_info *Screen, tree_node *Node, int ContainerType)
{
    Assert(Node, "CreateNodeContainer()")

    if(Node->SplitRatio == 0)
        Node->SplitRatio = KWMScreen.SplitRatio;

    switch(ContainerType)
    {
        case 1:
        {
            Node->Container = LeftVerticalContainerSplit(Screen, Node->Parent);
        } break;
        case 2:
        {
            Node->Container = RightVerticalContainerSplit(Screen, Node->Parent);
        } break;
        case 3:
        {
            Node->Container = UpperHorizontalContainerSplit(Screen, Node->Parent);
        } break;
        case 4:
        {
            Node->Container = LowerHorizontalContainerSplit(Screen, Node->Parent);
        } break;
    }

    Node->SplitMode = GetOptimalSplitMode(Node);
    Node->Container.Type = ContainerType;
}

void CreateNodeContainerPair(screen_info *Screen, tree_node *LeftNode, tree_node *RightNode, split_type SplitMode)
{
    Assert(LeftNode, "CreateNodeContainerPair() Left Node")
    Assert(RightNode, "CreateNodeContainerPair() Right Node")

    if(SplitMode == SPLIT_VERTICAL)
    {
        CreateNodeContainer(Screen, LeftNode, 1);
        CreateNodeContainer(Screen, RightNode, 2);
    }
    else
    {
        CreateNodeContainer(Screen, LeftNode, 3);
        CreateNodeContainer(Screen, RightNode, 4);
    }
}

bool IsPseudoNode(tree_node *Node)
{
    return Node && Node->WindowID == -1 && IsLeafNode(Node);
}

void CreatePseudoNode()
{
    screen_info *Screen = KWMScreen.Current;
    space_info *Space = GetActiveSpaceOfScreen(Screen);
    window_info *Window = KWMFocus.Window;
    if(!Screen || !Space || !Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(Space->RootNode, Window->WID);
    if(Node)
    {
        split_type SplitMode = KWMScreen.SplitMode == SPLIT_OPTIMAL ? GetOptimalSplitMode(Node) : KWMScreen.SplitMode;
        CreateLeafNodePair(Screen, Node, Node->WindowID, -1, SplitMode);
        ApplyTreeNodeContainer(Node);
    }
}

void RemovePseudoNode()
{
    screen_info *Screen = KWMScreen.Current;
    space_info *Space = GetActiveSpaceOfScreen(Screen);
    window_info *Window = KWMFocus.Window;
    if(!Screen || !Space || !Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(Space->RootNode, Window->WID);
    if(Node && Node->Parent)
    {
        tree_node *Parent = Node->Parent;
        tree_node *PseudoNode = IsLeftChild(Node) ? Parent->RightChild : Parent->LeftChild;
        if(!PseudoNode || !IsLeafNode(PseudoNode) || PseudoNode->WindowID != -1)
            return;

        Parent->WindowID = Node->WindowID;
        Parent->LeftChild = NULL;
        Parent->RightChild = NULL;
        free(Node);
        free(PseudoNode);
        ApplyTreeNodeContainer(Parent);
    }
}

tree_node *CreateLeafNode(screen_info *Screen, tree_node *Parent, int WindowID, int ContainerType)
{
    Assert(Parent, "CreateLeafNode()")

    tree_node Clear = {0};
    tree_node *Leaf = (tree_node*) malloc(sizeof(tree_node));
    *Leaf = Clear;

    Leaf->Parent = Parent;
    Leaf->WindowID = WindowID;
    Leaf->Type = NodeTypeTree;

    CreateNodeContainer(Screen, Leaf, ContainerType);

    Leaf->LeftChild = NULL;
    Leaf->RightChild = NULL;

    return Leaf;
}

tree_node *CreateRootNode()
{
    tree_node Clear = {0};
    tree_node *RootNode = (tree_node*) malloc(sizeof(tree_node));
    *RootNode = Clear;

    RootNode->WindowID = -1;
    RootNode->Type = NodeTypeTree;
    RootNode->Parent = NULL;
    RootNode->LeftChild = NULL;
    RootNode->RightChild = NULL;
    RootNode->SplitRatio = KWMScreen.SplitRatio;
    RootNode->SplitMode = SPLIT_OPTIMAL;

    return RootNode;
}

link_node *CreateLinkNode()
{
    link_node Clear = {0};
    link_node *Link = (link_node*) malloc(sizeof(link_node));
    *Link = Clear;

    Link->WindowID = -1;
    Link->Prev = NULL;
    Link->Next = NULL;

    return Link;
}

void SetRootNodeContainer(screen_info *Screen, tree_node *Node)
{
    Assert(Node, "SetRootNodeContainer()")

    space_info *Space = GetActiveSpaceOfScreen(Screen);

    Node->Container.X = Screen->X + Space->Settings.Offset.PaddingLeft;
    Node->Container.Y = Screen->Y + Space->Settings.Offset.PaddingTop;
    Node->Container.Width = Screen->Width - Space->Settings.Offset.PaddingLeft - Space->Settings.Offset.PaddingRight;
    Node->Container.Height = Screen->Height - Space->Settings.Offset.PaddingTop - Space->Settings.Offset.PaddingBottom;
    Node->SplitMode = GetOptimalSplitMode(Node);

    Node->Container.Type = 0;
}

void SetLinkNodeContainer(screen_info *Screen, link_node *Link)
{
    Assert(Link, "SetRootNodeContainer()")

    space_info *Space = GetActiveSpaceOfScreen(Screen);

    Link->Container.X = Screen->X + Space->Settings.Offset.PaddingLeft;
    Link->Container.Y = Screen->Y + Space->Settings.Offset.PaddingTop;
    Link->Container.Width = Screen->Width - Space->Settings.Offset.PaddingLeft - Space->Settings.Offset.PaddingRight;
    Link->Container.Height = Screen->Height - Space->Settings.Offset.PaddingTop - Space->Settings.Offset.PaddingBottom;
}
void CreateLeafNodePair(screen_info *Screen, tree_node *Parent, int FirstWindowID, int SecondWindowID, split_type SplitMode)
{
    Assert(Parent, "CreateLeafNodePair()")

    Parent->WindowID = -1;
    Parent->SplitMode = SplitMode;
    Parent->SplitRatio = KWMScreen.SplitRatio;

    node_type ParentType = Parent->Type;
    link_node *ParentList = Parent->List;
    Parent->Type = NodeTypeTree;
    Parent->List = NULL;

    int LeftWindowID = KWMTiling.SpawnAsLeftChild ? SecondWindowID : FirstWindowID;
    int RightWindowID = KWMTiling.SpawnAsLeftChild ? FirstWindowID : SecondWindowID;

    if(SplitMode == SPLIT_VERTICAL)
    {
        Parent->LeftChild = CreateLeafNode(Screen, Parent, LeftWindowID, 1);
        Parent->RightChild = CreateLeafNode(Screen, Parent, RightWindowID, 2);

        tree_node *Node = KWMTiling.SpawnAsLeftChild ?  Parent->RightChild : Parent->LeftChild;
        Node->Type = ParentType;
        Node->List = ParentList;
        ResizeLinkNodeContainers(Node);
    }
    else if(SplitMode == SPLIT_HORIZONTAL)
    {
        Parent->LeftChild = CreateLeafNode(Screen, Parent, LeftWindowID, 3);
        Parent->RightChild = CreateLeafNode(Screen, Parent, RightWindowID, 4);

        tree_node *Node = KWMTiling.SpawnAsLeftChild ?  Parent->RightChild : Parent->LeftChild;
        Node->Type = ParentType;
        Node->List = ParentList;
        ResizeLinkNodeContainers(Node);
    }
    else
    {
        Parent->Parent = NULL;
        Parent->LeftChild = NULL;
        Parent->RightChild = NULL;
        Parent = NULL;
    }
}

bool IsLeafNode(tree_node *Node)
{
    return Node->LeftChild == NULL && Node->RightChild == NULL ? true : false;
}

void GetFirstLeafNode(tree_node *Node, void **Result)
{
    if(Node)
    {
        if(Node->Type == NodeTypeLink)
            *Result = Node->List;

        else if(Node->Type == NodeTypeTree)
        {
            while(Node->LeftChild)
                Node = Node->LeftChild;

            *Result = Node;
        }
    }
}

void GetLastLeafNode(tree_node *Node, void **Result)
{
    if(Node)
    {
        if(Node->Type == NodeTypeLink)
        {
            link_node *Link = Node->List;
            while(Link->Next)
                Link = Link->Next;

            *Result = Link;
        }
        else if(Node->Type == NodeTypeTree)
        {
            while(Node->RightChild)
                Node = Node->RightChild;

            *Result = Node;
        }
    }
}

tree_node *GetFirstPseudoLeafNode(tree_node *Node)
{
    tree_node *Leaf = NULL;
    GetFirstLeafNode(Node, (void**)&Leaf);
    while(Leaf && Leaf->WindowID != -1)
        Leaf = GetNearestTreeNodeToTheRight(Leaf);

    return Leaf;
}

bool IsLeftChild(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
    {
        tree_node *Parent = Node->Parent;
        return Parent->LeftChild == Node;
    }

    return false;
}

bool IsRightChild(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
    {
        tree_node *Parent = Node->Parent;
        return Parent->RightChild == Node;
    }

    return false;
}

tree_node *CreateTreeFromWindowIDList(screen_info *Screen, std::vector<window_info*> *WindowsPtr)
{
    if(IsSpaceFloating(Screen->ActiveSpace))
        return NULL;

    tree_node *RootNode = CreateRootNode();
    SetRootNodeContainer(Screen, RootNode);

    bool Result = false;
    space_info *Space = GetActiveSpaceOfScreen(Screen);

    if(Space->Settings.Mode == SpaceModeBSP)
        Result = CreateBSPTree(RootNode, Screen, WindowsPtr);
    else if(Space->Settings.Mode == SpaceModeMonocle)
        Result = CreateMonocleTree(RootNode, Screen, WindowsPtr);

    if(!Result)
    {
        free(RootNode);
        RootNode = NULL;
    }

    return RootNode;
}

bool CreateBSPTree(tree_node *RootNode, screen_info *Screen, std::vector<window_info*> *WindowsPtr)
{
    Assert(RootNode, "CreateBSPTree()")

    bool Result = false;
    std::vector<window_info*> &Windows = *WindowsPtr;

    if(!Windows.empty())
    {
        tree_node *Root = RootNode;
        Root->WindowID = Windows[0]->WID;
        for(std::size_t WindowIndex = 1; WindowIndex < Windows.size(); ++WindowIndex)
        {
            while(!IsLeafNode(Root))
            {
                if(!IsLeafNode(Root->LeftChild) && IsLeafNode(Root->RightChild))
                    Root = Root->RightChild;
                else
                    Root = Root->LeftChild;
            }

            DEBUG("CreateBSPTree() Create pair of leafs")
            CreateLeafNodePair(Screen, Root, Root->WindowID, Windows[WindowIndex]->WID, GetOptimalSplitMode(Root));
            Root = RootNode;
        }

        Result = true;
    }

    return Result;
}

bool CreateMonocleTree(tree_node *RootNode, screen_info *Screen, std::vector<window_info*> *WindowsPtr)
{
    Assert(RootNode, "CreateMonocleTree()")

    bool Result = false;
    std::vector<window_info*> &Windows = *WindowsPtr;

    if(!Windows.empty())
    {
        tree_node *Root = RootNode;
        Root->List = CreateLinkNode();

        SetLinkNodeContainer(Screen, Root->List);
        Root->List->WindowID = Windows[0]->WID;

        link_node *Link = Root->List;
        for(std::size_t WindowIndex = 1; WindowIndex < Windows.size(); ++WindowIndex)
        {
            link_node *Next = CreateLinkNode();
            SetLinkNodeContainer(Screen, Next);
            Next->WindowID = Windows[WindowIndex]->WID;

            Link->Next = Next;
            Next->Prev = Link;
            Link = Next;
        }

        Result = true;
    }

    return Result;
}

split_type GetOptimalSplitMode(tree_node *Node)
{
    return (Node->Container.Width / Node->Container.Height) >= KWMTiling.OptimalRatio ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;
}

void ChangeSplitRatio(double Value)
{
    if(Value > 0.0 && Value < 1.0)
    {
        DEBUG("ChangeSplitRatio() New Split-Ratio is " << Value)
        KWMScreen.SplitRatio = Value;
    }
}

void SwapNodeWindowIDs(tree_node *A, tree_node *B)
{
    if(A && B)
    {
        DEBUG("SwapNodeWindowIDs() " << A->WindowID << " with " << B->WindowID)
        int TempWindowID = A->WindowID;
        A->WindowID = B->WindowID;
        B->WindowID = TempWindowID;

        node_type TempNodeType = A->Type;
        A->Type = B->Type;
        B->Type = TempNodeType;

        link_node *TempLinkList = A->List;
        A->List = B->List;
        B->List = TempLinkList;

        ResizeLinkNodeContainers(A);
        ResizeLinkNodeContainers(B);
        ApplyTreeNodeContainer(A);
        ApplyTreeNodeContainer(B);
    }
}

void SwapNodeWindowIDs(link_node *A, link_node *B)
{
    if(A && B)
    {
        DEBUG("SwapNodeWindowIDs() " << A->WindowID << " with " << B->WindowID)
        int TempWindowID = A->WindowID;
        A->WindowID = B->WindowID;
        B->WindowID = TempWindowID;
        ResizeWindowToContainerSize(A);
        ResizeWindowToContainerSize(B);
    }
}

void ToggleTypeOfFocusedNode()
{
    space_info *Space = GetActiveSpaceOfScreen(KWMScreen.Current);
    tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(Space->RootNode, KWMFocus.Window->WID);
    if(TreeNode && TreeNode != Space->RootNode)
        TreeNode->Type = TreeNode->Type == NodeTypeTree ? NodeTypeLink : NodeTypeTree;
}

void ChangeTypeOfFocusedNode(node_type Type)
{
    Assert(KWMScreen.Current, "ChangeTypeOfFocusedTreeNode() KWMScreen.Current");
    Assert(KWMFocus.Window, "ChangeTypeOfFocusedTreeNode() KWMFocus.Window");

    space_info *Space = GetActiveSpaceOfScreen(KWMScreen.Current);
    tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(Space->RootNode, KWMFocus.Window->WID);
    if(TreeNode && TreeNode != Space->RootNode)
        TreeNode->Type = Type;
}

tree_node *GetNearestLeafNodeNeighbour(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
        return IsLeftChild(Node) ? GetNearestTreeNodeToTheRight(Node) : GetNearestTreeNodeToTheLeft(Node);

    return NULL;
}

tree_node *GetTreeNodeFromWindowID(tree_node *Node, int WindowID)
{
    if(Node)
    {
        tree_node *CurrentNode = NULL;
        GetFirstLeafNode(Node, (void**)&CurrentNode);
        while(CurrentNode)
        {
            if(CurrentNode->WindowID == WindowID)
                return CurrentNode;

            CurrentNode = GetNearestTreeNodeToTheRight(CurrentNode);
        }
    }

    return NULL;
}

tree_node *GetTreeNodeFromWindowIDOrLinkNode(tree_node *Node, int WindowID)
{
    tree_node *Result = NULL;
    Result = GetTreeNodeFromWindowID(Node, WindowID);
    if(!Result)
    {
        link_node *Link = GetLinkNodeFromWindowID(Node, WindowID);
        Result = GetTreeNodeFromLink(Node, Link);
    }

    return Result;
}

link_node *GetLinkNodeFromWindowID(tree_node *Root, int WindowID)
{
    if(Root)
    {
        tree_node *Node = NULL;
        GetFirstLeafNode(Root, (void**)&Node);
        while(Node)
        {
            link_node *Link = GetLinkNodeFromTree(Node, WindowID);
            if(Link)
                return Link;

            Node = GetNearestTreeNodeToTheRight(Node);
        }
    }

    return NULL;
}

link_node *GetLinkNodeFromTree(tree_node *Root, int WindowID)
{
    if(Root)
    {
        link_node *Link = Root->List;
        while(Link)
        {
            if(Link->WindowID == WindowID)
                return Link;

            Link = Link->Next;
        }
    }

    return NULL;
}

tree_node *GetTreeNodeFromLink(tree_node *Root, link_node *Link)
{
    if(Root && Link)
    {
        tree_node *Node = NULL;
        GetFirstLeafNode(Root, (void**)&Node);
        while(Node)
        {
            if(GetLinkNodeFromTree(Node, Link->WindowID) == Link)
                return Node;

            Node = GetNearestTreeNodeToTheRight(Node);
        }
    }

    return NULL;
}

void ResizeNodeContainer(screen_info *Screen, tree_node *Node)
{
    if(Node)
    {
        if(Node->LeftChild)
        {
            CreateNodeContainer(Screen, Node->LeftChild, Node->LeftChild->Container.Type);
            ResizeNodeContainer(Screen, Node->LeftChild);
            ResizeLinkNodeContainers(Node->LeftChild);
        }

        if(Node->RightChild)
        {
            CreateNodeContainer(Screen, Node->RightChild, Node->RightChild->Container.Type);
            ResizeNodeContainer(Screen, Node->RightChild);
            ResizeLinkNodeContainers(Node->RightChild);
        }
    }
}

void ResizeLinkNodeContainers(tree_node *Root)
{
    if(Root)
    {
        link_node *Link = Root->List;
        while(Link)
        {
            Link->Container = Root->Container;
            Link = Link->Next;
        }
    }
}

tree_node *GetNearestTreeNodeToTheLeft(tree_node *Node)
{
    if(Node)
    {
        if(Node->Parent)
        {
            tree_node *Root = Node->Parent;
            if(Root->LeftChild == Node)
                return GetNearestTreeNodeToTheLeft(Root);

            if(IsLeafNode(Root->LeftChild))
                return Root->LeftChild;

            Root = Root->LeftChild;
            while(!IsLeafNode(Root->RightChild))
                Root = Root->RightChild;

            return Root->RightChild;
        }
    }

    return NULL;
}

tree_node *GetNearestTreeNodeToTheRight(tree_node *Node)
{
    if(Node)
    {
        if(Node->Parent)
        {
            tree_node *Root = Node->Parent;
            if(Root->RightChild == Node)
                return GetNearestTreeNodeToTheRight(Root);

            if(IsLeafNode(Root->RightChild))
                return Root->RightChild;

            Root = Root->RightChild;
            while(!IsLeafNode(Root->LeftChild))
                Root = Root->LeftChild;

            return Root->LeftChild;
        }
    }

    return NULL;
}

void CreateNodeContainers(screen_info *Screen, tree_node *Node, bool OptimalSplit)
{
    if(Node && Node->LeftChild && Node->RightChild)
    {
        Node->SplitMode = OptimalSplit ? GetOptimalSplitMode(Node) : Node->SplitMode;
        CreateNodeContainerPair(Screen, Node->LeftChild, Node->RightChild, Node->SplitMode);

        CreateNodeContainers(Screen, Node->LeftChild, OptimalSplit);
        CreateNodeContainers(Screen, Node->RightChild, OptimalSplit);
    }
}

void ToggleNodeSplitMode(screen_info *Screen, tree_node *Node)
{
    if(!Node || IsLeafNode(Node))
        return;

    Node->SplitMode = Node->SplitMode == SPLIT_VERTICAL ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;
    CreateNodeContainers(Screen, Node, false);
    ApplyTreeNodeContainer(Node);
}

void ApplyLinkNodeContainer(link_node *Link)
{
    if(Link)
    {
        ResizeWindowToContainerSize(Link);
        if(Link->Next)
            ApplyLinkNodeContainer(Link->Next);
    }
}

void ApplyTreeNodeContainer(tree_node *Node)
{
    if(Node)
    {
        if(Node->WindowID != -1)
            ResizeWindowToContainerSize(Node);

        if(Node->List)
            ApplyLinkNodeContainer(Node->List);

        if(Node->LeftChild)
            ApplyTreeNodeContainer(Node->LeftChild);

        if(Node->RightChild)
            ApplyTreeNodeContainer(Node->RightChild);
    }
}

void DestroyLinkList(link_node *Link)
{
    if(Link)
    {
        if(Link->Next)
            DestroyLinkList(Link->Next);

        free(Link);
        Link = NULL;
    }
}

void DestroyNodeTree(tree_node *Node)
{
    if(Node)
    {
        if(Node->List)
            DestroyLinkList(Node->List);

        if(Node->LeftChild)
            DestroyNodeTree(Node->LeftChild);

        if(Node->RightChild)
            DestroyNodeTree(Node->RightChild);

        free(Node);
        Node = NULL;
    }
}

void RotateTree(tree_node *Node, int Deg)
{
    if (Node == NULL || IsLeafNode(Node))
        return;

    DEBUG("RotateTree() " << Deg << " degrees")

    if((Deg == 90 && Node->SplitMode == SPLIT_VERTICAL) ||
       (Deg == 270 && Node->SplitMode == SPLIT_HORIZONTAL) ||
       Deg == 180)
    {
        tree_node *Temp = Node->LeftChild;
        Node->LeftChild = Node->RightChild;
        Node->RightChild = Temp;
        Node->SplitRatio = 1 - Node->SplitRatio;
    }

    if(Deg != 180)
        Node->SplitMode = Node->SplitMode == SPLIT_HORIZONTAL ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;

    RotateTree(Node->LeftChild, Deg);
    RotateTree(Node->RightChild, Deg);
}

void CreateDeserializedNodeContainer(tree_node *Node)
{
    int SplitMode = Node->Parent->SplitMode;
    int ContainerType = 0;

    if(SplitMode == SPLIT_VERTICAL)
        ContainerType = IsLeftChild(Node) ? 1 : 2;
    else
        ContainerType = IsLeftChild(Node) ? 3 : 4;

    CreateNodeContainer(KWMScreen.Current, Node, ContainerType);
}

void FillDeserializedTree(tree_node *RootNode)
{
    std::vector<window_info*> Windows = GetAllWindowsOnDisplay(KWMScreen.Current->ID);
    tree_node *Current = NULL;
    GetFirstLeafNode(RootNode, (void**)&Current);

    std::size_t Counter = 0, Leafs = 0;
    while(Current)
    {
        if(Counter < Windows.size())
            Current->WindowID = Windows[Counter++]->WID;

        Current = GetNearestTreeNodeToTheRight(Current);
        ++Leafs;
    }

    if(Leafs < Windows.size() && Counter < Windows.size())
    {
        tree_node *Root = RootNode;
        for(; Counter < Windows.size(); ++Counter)
        {
            while(!IsLeafNode(Root))
            {
                if(!IsLeafNode(Root->LeftChild) && IsLeafNode(Root->RightChild))
                    Root = Root->RightChild;
                else
                    Root = Root->LeftChild;
            }

            DEBUG("FillDeserializedTree() Create pair of leafs")
            CreateLeafNodePair(KWMScreen.Current, Root, Root->WindowID, Windows[Counter]->WID, GetOptimalSplitMode(Root));
            Root = RootNode;
        }
    }
}
